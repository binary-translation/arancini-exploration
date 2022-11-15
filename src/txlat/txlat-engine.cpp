#include <arancini/elf/elf-reader.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <arancini/txlat/txlat-engine.h>
#include <arancini/util/tempfile-manager.h>
#include <arancini/util/tempfile.h>
#include <chrono>
#include <cstdlib>
#include <iostream>

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
	std::cerr << "running: " << cmd << "..." << std::endl;
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
	elf_reader elf(cmdline.at("input").as<std::string>());
	elf.parse();

	// TODO: Figure the input engine out from ELF architecture header
	auto das = cmdline.at("syntax").as<std::string>() == "att" ? disassembly_syntax::att : disassembly_syntax::intel;
	auto ia = std::make_unique<arancini::input::x86::x86_input_arch>(das);

	// Construct the output engine
	auto intermediate_file = tf.create_file(".o");
	auto oe = std::make_shared<arancini::output::o_static::llvm::llvm_static_output_engine>(intermediate_file->name());
	process_options(*oe, cmdline);

	if (!cmdline.count("no-static")) {
		// Loop over each symbol table, and translate the symbol.
		for (auto s : elf.sections()) {
			if (s->type() == section_type::symbol_table) {
				auto st = std::static_pointer_cast<symbol_table>(s);
				for (const auto &sym : st->symbols()) {
					// if (allowed_symbols.count(sym.name())) {
					if (sym.is_func()) {
						oe->add_chunk(translate_symbol(*ia, elf, sym));
					}
				}
			}
		}
	}

	// Determine if we also need to produce a dot graph output.
	if (cmdline.count("graph")) {
		std::string graph_output_file = cmdline.at("graph").as<std::string>();
		std::ostream *o;

		if (graph_output_file == "-") {
			o = &std::cout;
		} else {
			o = new std::ofstream(graph_output_file);
			if (!((std::ofstream *)o)->is_open()) {
				throw std::runtime_error("unable to open file for graph output");
			}
		}

		dot_graph_generator dgg(*o);
		for (auto c : oe->chunks()) {
			c->accept(dgg);
		}

		if (o != &std::cout) {
			delete o;
		}
	}

	// Invoke the output engine, and tell it to write to a temporary file.
	oe->generate();

	// Generate loadable sections
	std::vector<std::pair<std::shared_ptr<tempfile>, std::shared_ptr<program_header>>> phbins;

	// For each program header, determine whether or not it's loadable, and generate a
	// corresponding temporary file containing the binary contents of the segment.
	for (auto p : elf.program_headers()) {
		// std::cerr << "PH: " << (int)p->type() << std::endl;
		if (p->type() == program_header_type::loadable) {
			// Create a temporary file, and record it in the phbins list.
			auto phbin = tf.create_file(".bin");
			phbins.push_back({ phbin, p });

			// Open the file, and write the raw contents of the segment to it.
			auto s = phbin->open();
			s.write((const char *)p->data(), p->data_size());
		}
	}

	// Now, we need to create an assembly file that includes the binary data for
	// each program header, along with some associated metadata too.  TODO: Maybe we
	// should just include the original ELF file - but then the runtime would need to
	// parse and load it.
	auto phobjsrc = tf.create_file(".S");
	{
		auto s = phobjsrc->open();

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

	// Generate the final output binary by compiling everything together.
	run_or_fail(
		"g++ -o " + cmdline.at("output").as<std::string>() + " -no-pie " + intermediate_file->name() + " " + phobjsrc->name() + " -L out -larancini-runtime");
}

/*
This function uses an x86 implementation of the input_architecture class to lift
x86 symbol sections to the Arancini IR.
*/
std::shared_ptr<chunk> txlat_engine::translate_symbol(arancini::input::input_arch &ia, elf_reader &reader, const symbol &sym)
{
	// std::cerr << "translating symbol " << sym.name() << ", value=" << std::hex << sym.value() << ", size=" << sym.size() << ", section=" <<
	// sym.section_index()
	//		  << std::endl;

	auto section = reader.get_section(sym.section_index());
	if (!section) {
		throw std::runtime_error("unable to resolve symbol section");
	}

	off_t symbol_offset_in_section = sym.value() - section->address();

	const void *symbol_data = (const void *)((uintptr_t)section->data() + symbol_offset_in_section);

	default_ir_builder irb(true);

	auto start = std::chrono::high_resolution_clock::now();
	ia.translate_chunk(irb, sym.value(), symbol_data, sym.size(), false);
	auto dur = std::chrono::high_resolution_clock::now() - start;

	std::cerr << "symbol translation time: " << std::dec << std::chrono::duration_cast<std::chrono::microseconds>(dur).count() << " us" << std::endl;
	return irb.get_chunk();
}
