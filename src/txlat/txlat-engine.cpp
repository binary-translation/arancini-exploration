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

	if (!cmdline.count("no-static")) {
		// Loop over each symbol table, and translate the symbol.
		for (auto s : elf.sections()) {
			if (s->type() == section_type::symbol_table) {
				auto st = std::static_pointer_cast<symbol_table>(s);
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

	generate_guest_sections(phobjsrc, elf, load_phdrs, filename);

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
	const std::vector<std::shared_ptr<elf::program_header>> &load_phdrs, const std::basic_string<char> &filename)
{
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
}
