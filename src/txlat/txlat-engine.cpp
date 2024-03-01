#include <arancini/output/static/static-output-engine.h>
#include <arancini/util/logger.h>
#include <arancini/elf/elf-reader.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/opt.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <arancini/txlat/txlat-engine.h>
#include <arancini/util/tempfile-manager.h>
#include <arancini/util/tempfile.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace arancini::txlat;
using namespace arancini::elf;
using namespace arancini::ir;
using namespace arancini::input;
using namespace arancini::input::x86;
using namespace arancini::output;
using namespace arancini::output::o_static::llvm;
using namespace arancini::util;

static std::set<std::string> allowed_symbols
	= { "cmpstr", "cmpnum", "swap", "_qsort", "_start", "test", "__libc_start_main", "_dl_aux_init", "__assert_fail", "__dcgettext", "__dcigettext" };

void txlat_engine::process_options(arancini::output::o_static::static_output_engine &oe, const boost::program_options::variables_map &cmdline)
{
	if (auto llvmoe = dynamic_cast<llvm_static_output_engine *>(&oe)) {
		llvmoe->set_debug(cmdline.count("debug"));
	}
}

static void run_or_fail(const std::string &cmd)
{
    util::global_logger.info("Running: {}...\n", cmd);
	if (std::system(cmd.c_str()) != 0) {
		throw std::runtime_error("error whilst running subcommand");
	}
}

/*
This function acts as the main driver for the binary translation.
First it parses the ELF binary, lifting each section to the Arancini IR.
Finally, the lifted IR is processed by the output engine.
For example, the output engine can generate the target binary or a
visualisation of the Arancini IR.
*/
void txlat_engine::translate(const boost::program_options::variables_map &cmdline)
{
	// Create a manager for temporary files, as we'll be creating a series of them.  When
	// this object is destroyed, all temporary files are automatically unlinked.
	tempfile_manager tf;

	// Parse the input ELF file
	const auto &filename = cmdline.at("input").as<std::string>();
	elf_reader elf(filename);
	elf.parse();

	// TODO: Figure the input engine out from ELF architecture header
	auto das = cmdline.at("syntax").as<std::string>() == "att" ? disassembly_syntax::att : disassembly_syntax::intel;
	auto ia = std::make_unique<arancini::input::x86::x86_input_arch>(cmdline.count("debug") || cmdline.count("graph"), das);

	// Construct the output engine
	auto intermediate_file = tf.create_file(".o");
	auto oe = std::make_shared<arancini::output::o_static::llvm::llvm_static_output_engine>(intermediate_file->name());
	process_options(*oe, cmdline);

	oe->set_entrypoint(elf.get_entrypoint());

	std::shared_ptr<symbol_table> dyn_sym;
	std::vector<std::shared_ptr<rela_table>> relocations;

	// Loop over each symbol table, and translate the symbol.
	for (auto &s : elf.sections()) {
		if (s->type() == elf::section_type::dynamic_symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			dyn_sym = std::move(st);
		} else if (s->type() == elf::section_type::relocation_addend) {
			auto st = std::static_pointer_cast<rela_table>(s);
			relocations.push_back(std::move(st));
		} else if (s->type() == section_type::symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			if (!cmdline.count("no-static")) {
				for (const auto &sym : st->symbols()) {
					// if (allowed_symbols.count(sym.name())) {
					if (sym.is_func() && sym.size()) {
						oe->add_chunk(translate_symbol(*ia, elf, sym));
					}
				}
			}
		}
	}

	// Generate a dot graph of the IR if required
	if (cmdline.count("graph")) {
    generate_dot_graph(*oe, cmdline.at("graph").as<std::string>());
	}

  // Execute required optimisations from the command line
  optimise(*oe, cmdline);

	// Generate a dot graph of the optimized IR if required
	if (cmdline.count("graph")) {
    std::string opt_filename = cmdline.at("graph").as<std::string>();
    opt_filename = opt_filename.substr(0, opt_filename.find_last_of('.'));
    opt_filename += ".opt.dot";
    generate_dot_graph(*oe, opt_filename);
	}

	// If the main output command-line option was not specified, then don't go any further.
	if (!cmdline.count("output")) {
		return;
	}

	// Invoke the output engine, and tell it to write to a temporary file.
	oe->generate();

	// --------------- //

	// An output file was specified, so continue to build the translated binary.
	std::string cxx_compiler = cmdline.at("cxx-compiler-path").as<std::string>();

	if (cmdline.count("wrapper")) {
		cxx_compiler = cmdline.at("wrapper").as<std::string>() + " " + cxx_compiler;
	}

	std::string arancini_runtime_lib_path = cmdline.at("runtime-lib-path").as<std::string>();
	auto dir_start = arancini_runtime_lib_path.rfind("/");
	std::string arancini_runtime_lib_dir = arancini_runtime_lib_path.substr(0, dir_start);

	std::string debug_info = cmdline.count("debug-gen") ? " -g" : " -O3";

	std::string verbose_link = cmdline.count("verbose-link") ? " -Wl,--verbose" : "";

	if (cmdline.count("no-script")) {
		run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie " + intermediate_file->name() + " -l arancini-runtime -L "
			+ arancini_runtime_lib_dir + " -Wl,-rpath=" + arancini_runtime_lib_dir + debug_info + verbose_link);
		return;
	}

	// Generate loadable sections
	std::vector<std::shared_ptr<program_header>> load_phdrs;

	// For each program header, determine whether or not it's loadable, and generate a
	// corresponding temporary file containing the binary contents of the segment.
	for (const auto &p : elf.program_headers()) {
		if (p->type() == program_header_type::loadable) {
			load_phdrs.push_back(p);
		}
	}

	// Now, we need to create an assembly file that includes the binary data for
	// each program header, along with some associated metadata too.  TODO: Maybe we
	// should just include the original ELF file - but then the runtime would need to
	// parse and load it.
	auto phobjsrc = tf.create_file(".S");

	generate_guest_sections(phobjsrc, elf, load_phdrs, filename, dyn_sym, relocations, relocations_r);

	if (!cmdline.count("static-binary")) {


		// Generate the final output binary by compiling everything together.
		run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie " + intermediate_file->name() + " " + phobjsrc->name()
			+ " -l arancini-runtime -L " + arancini_runtime_lib_dir + " -Wl,-T,exec.lds,-rpath=" + arancini_runtime_lib_dir + debug_info);

	} else {
		std::string arancini_runtime_lib_dir = cmdline.at("static-binary").as<std::string>();

		// Generate the final output binary by compiling everything together.
		run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie -static-libgcc -static-libstdc++ " + intermediate_file->name()
			+ " " + phobjsrc->name() + " -L " + arancini_runtime_lib_dir
			+ " -l arancini-runtime-static -l arancini-input-x86-static -l arancini-output-riscv64-static -l arancini-ir-static" + " -L "
			+ arancini_runtime_lib_dir + "/../../obj -l xed" + debug_info + " -Wl,-T,exec.lds,-rpath=" + arancini_runtime_lib_dir);
	}


	// Patch relocations in result binary
	const auto &output = cmdline.at("output").as<std::string>();
	elf_reader elf1 = { output };

	elf1.parse();

	std::shared_ptr<symbol_table> generated_dynsym;
	std::vector<std::shared_ptr<rela_table>> generated_rela;

	for (auto &s : elf1.sections()) {
		if (s->type() == elf::section_type::dynamic_symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			generated_dynsym = std::move(st);
		} else if (s->type() == elf::section_type::relocation_addend) {
			auto st = std::static_pointer_cast<rela_table>(s);
			generated_rela.push_back(std::move(st));
		}
	}

	std::map<std::string, int> guest_symbol_to_index;

	const std::vector<symbol> &guest_symbols = generated_dynsym->symbols();
	for (size_t i = 0; i < guest_symbols.size(); ++i) {
		const std::string &string = guest_symbols[i].name();
		if (string.size() >= 9) {
			guest_symbol_to_index.emplace(string.substr(9), i);
		}
	}

	{
		std::ofstream file(cmdline.at("output").as<std::string>(), std::ios::out | std::ios::binary | std::ios::in);

		for (const auto &relocs : generated_rela) {
			const std::vector<rela> &relocations1 = relocs->relocations();
			for (size_t i = 0; i < relocations1.size(); ++i) {
				const auto &reloc = relocations1[i];
				unsigned int transform = reloc.type() & 0xf0000000;
				if (transform == 0x10000000) { // symbol index needs to be adjusted to point to correct index in target dyn_sym table
					unsigned int new_symbol = guest_symbol_to_index[dyn_sym->symbols()[reloc.symbol()].name()];
					unsigned int buf = reloc.type() & ~0xf0000000;
					file.seekp(relocs->file_offset() + 24 * i + 8);
					file.write(reinterpret_cast<const char *>(&buf), sizeof(buf));
					file.write(reinterpret_cast<const char *>(&new_symbol), sizeof(new_symbol));
					// Write new_symbol to (relocs.file_offset() + 24 * i + 12) and reloc.type() & ~0xf0000000 to (relocs.file_offset() + 24 * i + 8)
				} else if (transform) {
					throw std::runtime_error("Invalid relocation transform type " + std::to_string(transform));
				}
			}
		}
		file.close();
	}
}

void txlat_engine::add_symbol_to_output(
	const std::vector<std::shared_ptr<program_header>> &phbins, const std::map<off_t, unsigned int> &end_addresses, const symbol &sym, std::ofstream &s)
{
	auto type = sym.type();

	unsigned int i = end_addresses.upper_bound(sym.value())->second;
	const std::shared_ptr<program_header> &phdr = phbins[i / 2];

	auto name = "__guest__" + sym.name();
	if (sym.section_index() != SHN_UNDEF) {
		s << ".set \"" << name << "\", __PH_" << std::dec << i / 2 << "_DATA_" << i % 2 << " + "
		  << sym.value() - phdr->address() - (i % 2) * (phdr->data_size()) << '\n'
		  << ".size \"" << name << "\", " << std::dec << sym.size() << '\n';
	}
	if (sym.is_global()) {
		s << ".globl \"" << name << "\"\n";
	} else if (sym.is_weak()) {
		s << ".weak \"" << name << "\"\n";
	}

	s << ".type \"" << name << "\", " << type << '\n';
	if (sym.is_hidden()) {
		s << ".hidden \"" << name << "\"\n";
	} else if (sym.is_internal()) {
		s << ".internal \"" << name << "\"\n";
	} else if (sym.is_protected()) {
		s << ".protected \"" << name << "\"\n";
	}
}

/*
This function uses an x86 implementation of the input_architecture class to lift
x86 symbol sections to the Arancini IR.
*/
std::shared_ptr<chunk> txlat_engine::translate_symbol(arancini::input::input_arch &ia, elf_reader &reader, const symbol &sym)
{
    ::util::global_logger.info("Translating symbol {}; value={:x} size={} section={}\n", sym.name(), sym.value(), sym.size(), sym.section_index());

	auto section = reader.get_section(sym.section_index());
	if (!section) {
		throw std::runtime_error("unable to resolve symbol section");
	}

	off_t symbol_offset_in_section = sym.value() - section->address();

	const void *symbol_data = (const void *)((uintptr_t)section->data() + symbol_offset_in_section);

	default_ir_builder irb(ia.get_internal_function_resolver(), true);

	auto start = std::chrono::high_resolution_clock::now();
	ia.translate_chunk(irb, sym.value(), symbol_data, sym.size(), false);
	auto dur = std::chrono::high_resolution_clock::now() - start;

    ::util::global_logger.info("Symbol translation time: {} us\n", std::chrono::duration_cast<std::chrono::microseconds>(dur).count());
	return irb.get_chunk();
}


void txlat_engine::generate_dot_graph(arancini::output::o_static::static_output_engine &oe, std::string filename)
{
	std::ostream *o;

    std::cout << "Generating dot graph to: " << filename << std::endl;

	if (filename == "-") {
		o = &std::cout;
	} else {
		o = new std::ofstream(filename);
		if (!((std::ofstream *)o)->is_open()) {
			throw std::runtime_error("unable to open file for graph output");
		}
	}

	dot_graph_generator dgg(*o);
	for (auto c : oe.chunks()) {
		c->accept(dgg);
	}

	if (o != &std::cout) {
		delete o;
	}
}

void txlat_engine::optimise(arancini::output::o_static::static_output_engine &oe, const boost::program_options::variables_map &cmdline)
{
    auto start = std::chrono::high_resolution_clock::now();
    deadflags_opt_visitor deadflags;
    for (auto c : oe.chunks()) {
      c->accept(deadflags);
    }
  	auto dur = std::chrono::high_resolution_clock::now() - start;
    ::util::global_logger.info("Optimisation: dead flags elimination pass took {} us\n", std::chrono::duration_cast<std::chrono::microseconds>(dur).count());
}

void txlat_engine::generate_guest_sections(const std::shared_ptr<util::tempfile> &phobjsrc, elf::elf_reader &elf,
	const std::vector<std::shared_ptr<elf::program_header>> &load_phdrs, const std::basic_string<char> &filename, const std::shared_ptr<symbol_table> &dyn_sym,
	const std::vector<std::shared_ptr<elf::rela_table>> &relocations)
{
	std::map<off_t, unsigned int> end_addresses;
	auto s = phobjsrc->open();

	// FIXME Currently hardcoded to current directory. Not sure what else to do since the linker script needs to have this path in it.
	auto l = std::ofstream("guest-sections.lds");


	// For each segment...
	for (unsigned int i = 0; i < load_phdrs.size(); i++) {

		auto phdr = load_phdrs[i];
		end_addresses[phdr->address() + phdr->data_size()] = 2 * i;

		off_t address = phdr->address();

		// Add 2 sections per PT_LOAD header (one for initialized and one for uninitialized data.
		l << ".gph.load" << std::dec << i << ".1 " << address << ": { *("
		  << ".gph.load" << std::dec << i << ".1) } :gphdr" << std::dec << i << '\n';
		l << ".gph.load" << std::dec << i << ".2 : { *("
		  << ".gph.load" << std::dec << i << ".2) } :gphdr" << std::dec << i << '\n';

		s << ".section .gph.load" << std::dec << i << ".1, \"a";
		if (phdr->flags() & PF_W) {
			s << 'w';
		}
		//			if (phdr->flags() & PF_X) {
		//				s << 'x';
		//			}
		s << "\"\n";

		s << "__PH_" << std::dec << i << "_DATA_0: .incbin \"" << filename << "\", " << phdr->offset() << ", " << phdr->data_size() << std::endl;
		if (phdr->mem_size() - phdr->data_size()) {
			end_addresses[phdr->address() + phdr->mem_size()] = 2 * i + 1;
			s << ".section .gph.load" << std::dec << i << ".2, \"a";
			if (phdr->flags() & PF_W) {
				s << 'w';
			}
			//				if (phdr->flags() & PF_X) {
			//					s << 'x';
			//				}
			s << "\", @nobits\n";
			// Using .zero n does not work with symbols
			s << "__PH_" << std::dec << i << "_DATA_1: .rept " << std::dec << phdr->mem_size() - phdr->data_size() << "\n.byte 0\n.endr\n";
		}
	}

	for (const auto &sym : dyn_sym->symbols()) {
		add_symbol_to_output(load_phdrs, end_addresses, sym, s);
	}

	// Manually emit relocations into the .grela section
	s << ".section .grela, \"a\"\n";

	// A Relocation consists of 3 quad words. First the offset, then type in the high 32 bit and symbol index in the low 32 bit of the second and the addend in
	// the third
	for (const auto &relocs : relocations) {
		for (const auto &reloc : relocs->relocations()) {
			if (reloc.is_relative()) {
				s << ".quad 0x" << std::hex << reloc.offset() << "\n.int 0x" << reloc.type_on_host() << "\n.int 0x" << reloc.symbol() << "\n.quad 0x"
				  << reloc.addend() << '\n';
			} else {
				// Mark this relocation as needing adjustment on the symbol index (it needs to match the index of the symbol in the generated binary).
				// Set the 4th highest bit of the type to indicate this.
				s << ".quad 0x" << std::hex << reloc.offset() << "\n.int 0x" << (0x10000000 | reloc.type_on_host()) << "\n.int 0x" << reloc.symbol()
				  << "\n.quad 0x" << reloc.addend() << '\n';
			}
		}
	}
}
