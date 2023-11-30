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
		if (auto filename = cmdline.at("dump-llvm"); !filename.empty()) {
			llvmoe->set_debug_dump_filename(filename.as<std::string>());
		}
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
	std::shared_ptr<symbol_table> sym_t;
	std::vector<std::shared_ptr<rela_table>> relocations;
	std::vector<std::shared_ptr<relr_array>> relocations_r;
	std::set<unsigned long> translated_addrs;
  	std::list<symbol> zero_size_symbol;
  	auto static_size = 0;

	// Loop over each symbol table, and translate the symbol.
	for (auto &s : elf.sections()) {
		if (s->type() == elf::section_type::dynamic_symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			dyn_sym = std::move(st);
		} else if (s->type() == elf::section_type::relocation_addend) {
			auto st = std::static_pointer_cast<rela_table>(s);
			relocations.push_back(std::move(st));
		} else if (s->type() == elf::section_type::relr) {
			auto st = std::static_pointer_cast<relr_array>(s);
			relocations_r.push_back(std::move(st));
		} else if (s->type() == section_type::symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			// TODO static translation of code in libraries
			if (!cmdline.count("no-static") && elf.type() == elf_type::exec) {
				for (const auto &sym : st->symbols()) {
          if (!sym.is_func())
            continue;
					std::cerr << "[DEBUG] PASS1: looking at symbol '" << sym.name() << "' @ 0x" << std::hex << sym.value() << std::endl;
					if (sym.is_func() && !translated_addrs.count(sym.value())) {
						translated_addrs.insert(sym.value());
            // Don't translate symbols with a size of 0, we'll do this in a second pass.
						if (!sym.size()) {
							std::cerr << "[DEBUG] PASS1: selecting for PASS2 (0 size), symbol '" << sym.name() << "'" << std::endl;
							zero_size_symbol.push_front(sym);
							continue;
						}
						std::cerr << "[DEBUG] PASS1: translating symbol '" << sym.name() << "' [" << std::dec << sym.size() << " bytes]" << std::endl;
						oe->add_chunk(translate_symbol(*ia, elf, sym));
            static_size += sym.size();
					} else {
            std::cerr << "[DEBUG] PASS1: address already seen for '" << sym.name() << "' @ 0x" << std::hex << sym.value() << std::endl;
          }
				}
			}
			sym_t = std::move(st);
		}
    // Loop over symbols of size zero and optimistically translate
		for (auto s : zero_size_symbol) {
			std::cerr << "[DEBUG] PASS2: looking at symbol '" << s.name() << "' @ 0x" << std::hex << s.value() << std::endl;
			// find the address of the symbol after s in the text section, and assume that the size of s is until there
			auto next = *(std::next(translated_addrs.find(s.value()), 1));
			auto size = next - s.value();
			auto fixed_sym = symbol(s.name(), s.value(), size, s.section_index(), s.info());
			std::cerr << "[DEBUG] PASS2: translating symbol '" << s.name() << "' [" << std::dec << size << " bytes]"  << std::endl;
			oe->add_chunk(translate_symbol(*ia, elf, fixed_sym));
			static_size += size;
		}
	}
  std::cout << "ahead-of-time: translated " << std::dec << static_size << " bytes" << std::endl;

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
		if (elf.type() == elf_type::exec) {
			run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie " + intermediate_file->name() + " -l arancini-runtime -L "
				+ arancini_runtime_lib_dir + " -Wl,-rpath=" + arancini_runtime_lib_dir + debug_info + verbose_link);
		} else if (elf.type() == elf::elf_type::dyn) {
			run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -shared " + intermediate_file->name() + +" -l arancini-runtime -L "
				+ arancini_runtime_lib_dir + " -Wl,-rpath=" + arancini_runtime_lib_dir + debug_info + verbose_link);
		}
		return;
	}

	// Generate loadable sections
	std::vector<std::shared_ptr<program_header>> load_phdrs;
	std::vector<std::shared_ptr<program_header>> tls;

	// For each program header, determine whether or not it's loadable, and generate a
	// corresponding temporary file containing the binary contents of the segment.
	for (const auto &p : elf.program_headers()) {
		if (p->type() == program_header_type::loadable) {
			load_phdrs.push_back(p);
		} else if (p->type() == program_header_type::tls) {
			tls.push_back(p);
		}
	}

	// Now, we need to create an assembly file that includes the binary data for each program header,
	// defines all dynsyms of the input binary with `__guest__` prefix, verbatim copies all relocations of the input binary and some metadata
	auto phobjsrc = tf.create_file(".S");

	std::map<uint64_t, std::string> ifuncs = generate_guest_sections(phobjsrc, elf, load_phdrs, filename, dyn_sym, relocations, relocations_r, sym_t, tls);
		// Put all segment data into the .gphdata section (guest program header data)
		s << ".section .gphdata,\"a\"" << std::endl;

		// For each segment...
		for (unsigned int i = 0; i < phbins.size(); i++) {
			// Create the metadata for the segment, which includes the load (virtual) address,
			// the actual byte size of the segment, the size in memory, then the actual data
			// itself.
			s << ".align 16" << std::endl;
			s << "__PH_" << std::dec << i << "_LOAD: .quad 0x" << std::hex << phbins[i].second->address() << std::endl;
			s << "__PH_" << std::dec << i << "_FSIZE: .quad 0x" << std::hex << phbins[i].second->data_size() << std::endl;
			s << "__PH_" << std::dec << i << "_MSIZE: .quad 0x" << std::hex << phbins[i].second->mem_size() << std::endl;
			s << "__PH_" << std::dec << i << "_DATA: .incbin \"" << phbins[i].first->name() << "\"" << std::endl;

			// Make sure the symbol is appropriately sized.
			s << ".size __PH_" << std::dec << i << "_DATA,.-__PH_" << std::dec << i << "_DATA" << std::endl;
		}

		// Finally, create pointers to each guest program header in an array.
		s << ".section .gph,\"a\"" << std::endl;
		s << ".globl __GPH" << std::endl;
		s << ".type __GPH,%object" << std::endl;
		s << "__GPH:" << std::endl;
		for (unsigned int i = 0; i < phbins.size(); i++) {
			s << ".quad __PH_" << std::dec << i << "_LOAD" << std::endl;
		}

		// Null terminate the array.
		s << ".quad 0" << std::endl;

		// Size the symbol appropriately.
		s << ".size __GPH,.-__GPH" << std::endl;
	}

	std::string cxx_compiler = cmdline.at("cxx-compiler-path").as<std::string>();

	if (cmdline.count("wrapper")) {
		cxx_compiler = cmdline.at("wrapper").as<std::string>() + " " + cxx_compiler;
	}

	std::string debug_info = cmdline.count("debug-gen") ? " -g" : "";
	std::string arancini_runtime_lib_path = cmdline.at("runtime-lib-path").as<std::string>();
	auto dir_start = arancini_runtime_lib_path.rfind("/");
	std::string arancini_runtime_lib_dir = arancini_runtime_lib_path.substr(0, dir_start);

	if (!cmdline.count("static-binary")) {
		std::string libs;

		if (cmdline.count("library")) {
			std::stringstream stringstream;
			for (const auto &item : cmdline.at("library").as<std::vector<std::string>>()) {
				stringstream << " " << item;
			}
			libs = stringstream.str();
		}

		if (elf.type() == elf::elf_type::exec) {
			// Generate the final output binary by compiling everything together.
			run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie " + intermediate_file->name() + libs + " " + phobjsrc->name()
				+ " -l arancini-runtime -L " + arancini_runtime_lib_dir + " -Wl,-T,exec.lds,-rpath=" + arancini_runtime_lib_dir + debug_info);
		} else if (elf.type() == elf::elf_type::dyn) {
			// Generate the final output library by compiling everything together.
			std::string tls_defines = tls.empty() ? ""
												  : " -DTLS_LEN=" + std::to_string(tls[0]->data_size()) + " -DTLS_SIZE=" + std::to_string(tls[0]->mem_size())
					+ " -DTLS_ALIGN=" + std::to_string(tls[0]->align());

			run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -shared " + intermediate_file->name() + " " + phobjsrc->name()
				+ tls_defines + " init_lib.c -l arancini-runtime -L " + arancini_runtime_lib_dir + libs + " -Wl,-T,lib.lds,-rpath=" + arancini_runtime_lib_dir
				+ debug_info);
		} else {
			throw std::runtime_error("Input elf type must be either an executable or shared object.");
		}

	} else {
		std::string arancini_runtime_lib_dir = cmdline.at("static-binary").as<std::string>();

		if (elf.type() != elf::elf_type::exec) {
			throw std::runtime_error("Can't generate a static binary from a shared object.");
		}

		// Generate the final output binary by compiling everything together.
		run_or_fail(cxx_compiler + " -o " + cmdline.at("output").as<std::string>() + " -no-pie -latomic -static-libgcc -static-libstdc++ " + intermediate_file->name()
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
				if (transform == 0x20000000) { // addend needs to be replaced with value of symbol with name ifuncs[addend].
					// ifunc tells us the symbol for the addend
					uint64_t new_addend = guest_symbols[guest_symbol_to_index[ifuncs[reloc.addend()].substr(9)]].value();
					unsigned int buf = reloc.type() & ~0xf0000000;
					file.seekp(relocs->file_offset() + 24 * i + 8);
					file.write(reinterpret_cast<const char *>(&buf), sizeof(buf));
					file.seekp(4, std::ios::cur);
					file.write(reinterpret_cast<const char *>(&new_addend), sizeof(new_addend));
					// Write new_addend to (relocs.file_offset() + 24 * i + 16) and reloc.type() & ~0xf0000000 to (relocs.file_offset() + 24 * i + 8)
				} else if (transform == 0x10000000) { // symbol index needs to be adjusted to point to correct index in target dyn_sym table
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

void txlat_engine::add_symbol_to_output(const std::vector<std::shared_ptr<program_header>> &phbins, const std::map<off_t, unsigned int> &end_addresses,
	const symbol &sym, std::ofstream &s, std::map<uint64_t, std::string> &ifuncs, bool force_global, bool omit_prefix)
{
	auto type = sym.type();

	unsigned int i = end_addresses.upper_bound(sym.value())->second;
	const std::shared_ptr<program_header> &phdr = phbins[i / 2];

	auto name = omit_prefix ? sym.name() : "__guest__" + sym.name();
	if (strcmp(type, "STT_GNU_IFUNC") == 0) {

		if (sym.section_index() != SHN_UNDEF) {
			s << ".set \"" << name << "__ifunc\", __PH_" << std::dec << i / 2 << "_DATA_" << i % 2 << " + "
			  << sym.value() - phdr->address() - (i % 2) * (phdr->data_size()) << '\n';
		}

		s << ".type \"" << name << "__ifunc\", "
		  << "STT_FUNC" << '\n'
		  << ".hidden \"" << name
		  << "__ifunc\"\n"

		  // Generate a stub that mimics a resolver with the following assembly code
		  /*						  name:
		   *  ff 25 00 00 00 00       jmp    QWORD PTR [rip+name_resolve]
		   *  57                      push   rdi
		   *  56                      push   rsi
		   *  52                      push   rdx
		   *  51                      push   rcx
		   *  41 50                   push   r8
		   *  51                      push   rcx
		   *  51                      push   rcx
		   *  e8 00 00 00 00          call   name_ifunc
		   *  41 59                   pop    r9
		   *  41 58                   pop    r8
		   *  59                      pop    rcx
		   *  5a                      pop    rdx
		   *  5e                      pop    rsi
		   *  5f                      pop    rdi
		   *  48 89 05 00 00 00 00    mov    QWORD PTR [rip+name_resolve],rax
		   *  ff e0                   jmp    rax
		   */

		  << ".section .data.resolve\n"
		  << '\"' << name << "__resolve\": .quad 1f\n"
		  << ".section .data.ifunc\n"
		  << '\"' << name << "\":\n"
		  << ".byte 0xff, 0x25\n"
		  << "0: .zero 4\n"
		  << "1: .byte 0x57, 0x56, 0x52, 0x51, 0x41, 0x50, 0x41, 0x51, 0xe8\n"
		  << "2: .zero 4\n"
		  << ".byte 0x41, 0x59, 0x41, 0x58, 0x59, 0x5a, 0x5e, 0x5f, 0x48, 0x89, 0x05\n"
		  << "3: .zero 4\n"
		  << ".byte 0xff, 0xe0\n"
		  << ".reloc 0b, R_RISCV_32_PCREL, \"" << name << "__resolve\" - 4\n"
		  << ".reloc 2b, R_RISCV_32_PCREL, \"" << name << "__ifunc\" - 4\n"
		  << ".reloc 3b, R_RISCV_32_PCREL, \"" << name << "__resolve\" - 4\n";

		type = "STT_FUNC";
		ifuncs[sym.value()] = name;

	} else if (sym.section_index() != SHN_UNDEF) {
		s << ".set \"" << name << "\", __PH_" << std::dec << i / 2 << "_DATA_" << i % 2 << " + "
		  << sym.value() - phdr->address() - (i % 2) * (phdr->data_size()) << '\n'
		  << ".size \"" << name << "\", " << std::dec << sym.size() << '\n';
	}
	if (force_global || sym.is_global()) {
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

std::map<uint64_t, std::string> txlat_engine::generate_guest_sections(const std::shared_ptr<util::tempfile> &phobjsrc, elf::elf_reader &elf,
	const std::vector<std::shared_ptr<elf::program_header>> &load_phdrs, const std::basic_string<char> &filename, const std::shared_ptr<symbol_table> &dyn_sym,
	const std::vector<std::shared_ptr<elf::rela_table>> &relocations, const std::vector<std::shared_ptr<elf::relr_array>> &relocations_r,
	const std::shared_ptr<symbol_table> &sym_t, const std::vector<std::shared_ptr<elf::program_header>> &tls)
{
	std::map<uint64_t, std::string> ifuncs;
	std::map<off_t, unsigned int> end_addresses;
	auto s = phobjsrc->open();

	// FIXME Currently hardcoded to current directory. Not sure what else to do since the linker script needs to have this path in it.
	auto l = std::ofstream("guest-sections.lds");

	if (tls.size() == 1) {
		if (elf.type() == elf::elf_type::exec) {
			size_t offset = tls[0]->mem_size() + (tls[0]->align() - 1); // Assume maximum misalignment penalty. Probably actually less. Correct at runtime.
			s << ".section .data\nguest_exec_tls:\n"
			  << ".quad 0\n" // next = NULL
			  << ".quad guest_tls\n" // image = guest_tls
			  << ".quad " << std::dec << tls[0]->data_size() << '\n' // len
			  << ".quad " << tls[0]->mem_size() << '\n' // size
			  << ".quad " << tls[0]->align() << '\n' // align
			  << ".quad " << offset << '\n' // offset
			  << ".globl guest_exec_tls\n"
			  << "tls_offset:\n"
			  << ".quad " << offset << '\n'
			  << ".globl tls_offset\n";
		}
	} else if (tls.size() > 1) {
		throw std::runtime_error("More than 1 TLS PHDR unsupported");
	}

	// For each segment...
	for (unsigned int i = 0; i < load_phdrs.size(); i++) {

		auto phdr = load_phdrs[i];
		end_addresses[phdr->address() + phdr->data_size()] = 2 * i;

		off_t address = phdr->address();
		if (tls.size() == 1) {
			// Other cases handled below

			// TLS initialized data (.tdata) is part of one of the LOAD headers (typically at the start), add a symbol so we can initialize it at runtime.
			if (phdr->offset() == tls[0]->offset()) {
				s << "guest_tls:\n";
			}
		}

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

		s << ".ifndef guest_base\nguest_base:\n.globl guest_base\n.hidden guest_base\n";
		if (elf.type() == elf::elf_type::exec) {
			s << "guest_exec_base:\n.globl guest_exec_base\n";
		}
		s << ".endif\n";

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

	if (dyn_sym) {
		for (const auto &sym : dyn_sym->symbols()) {
			add_symbol_to_output(load_phdrs, end_addresses, sym, s, ifuncs);
		}
	}

	for (const auto &sym : sym_t->symbols()) {
		if (sym.name() == "_DYNAMIC") {

			add_symbol_to_output(load_phdrs, end_addresses, sym, s, ifuncs, true);
			s << ".hidden __guest___DYNAMIC\n";
			if (elf.type() == elf::elf_type::exec) {
				symbol sy { "guest_exec_DYNAMIC", sym.value(), sym.size(), sym.section_index(), sym.info(), 0 };
				add_symbol_to_output(load_phdrs, end_addresses, sy, s, ifuncs, true, true);
			}
		}
		static const std::set<std::string> symbols_to_copy_global { "main_ctor_queue", "__malloc_replaced", "__libc", "__thread_list_lock", "__sysinfo" };
		if (symbols_to_copy_global.count(sym.name())) {
			add_symbol_to_output(load_phdrs, end_addresses, sym, s, ifuncs, true);
		}
	}

	// Manually emit relocations into the .grela section
	s << ".section .grela, \"a\"\n";

	for (const auto &relocs : relocations_r) {
		for (const auto &reloc : relocs->relocations()) {
			unsigned int i = end_addresses.upper_bound(reloc)->second;
			const std::shared_ptr<program_header> &phdr = load_phdrs[i / 2];

			// Reading this here because now the file_offset can be calculated from the encompassing phdr
			uint64_t addend = elf.read_relr_addend(phdr->offset() - phdr->address() + reloc);

			// FIXME hardcoded 3 as RELATIVE reloc type
			s << ".quad 0x" << std::hex << reloc << "\n.quad 3\n.quad 0x" << addend << '\n';
		}
	}

	// A Relocation consists of 3 quad words. First the offset, then type in the high 32 bit and symbol index in the low 32 bit of the second and the addend in
	// the third
	for (const auto &relocs : relocations) {
		for (const auto &reloc : relocs->relocations()) {
			if (reloc.is_irelative()) {
				// Use a relative reloc to the stub function instead. Identify the needed function by the original addend.
				if (ifuncs.count(reloc.addend())) {
					s << ".quad 0x" << std::hex << reloc.offset() << "\n.quad 0x20000003\n.quad 0x" << reloc.addend() << '\n';
				}
			} else if (reloc.is_tpoff()) {
				// For TP relative relocations, we only know the offset after mapping, but the emulated TLS is separate from the host TLS, so we can only
				// perform those manually in library init. Add an array of offset, addend pairs to iterate over.

				if(elf.type() == elf_type::exec) {
					throw std::runtime_error("TP relative reloc in binary not supported.");
				}

				s << ".section .data.tp_reloc\n.ifndef __TPREL_INIT\n__TPREL_INIT:\n.endif\n"
				  << "0: .quad 0x" << std::hex << reloc.offset() << ", 0x" << reloc.addend() << '\n'
				  << ".reloc 0b, R_RISCV_64, 0x" << std::hex << reloc.offset() << '\n'
				  << ".section .grela\n";
			} else if (reloc.is_relative()) {
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

	s << ".section .data.tp_reloc\n.ifndef __TPREL_INIT\n__TPREL_INIT:\n.endif\n.quad 0x0, 0x0\n"
	  << ".globl __TPREL_INIT\n.type __TPREL_INIT, STT_OBJECT\n.size __TPREL_INIT, . - __TPREL_INIT \n.hidden __TPREL_INIT\n"
	  << ".ifndef guest_tls\n.set guest_tls, 0\n.endif\n.globl guest_tls\n.hidden guest_tls\n";
	if (elf.type() == elf_type::exec) {
		s << ".ifndef tls_offset\ntls_offset:\n.quad 0\n.globl tls_offset\n.endif\n";
	}

	return ifuncs;
}
