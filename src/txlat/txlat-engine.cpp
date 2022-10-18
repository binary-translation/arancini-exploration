#include <arancini/elf/elf-reader.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <arancini/output/output-personality.h>

#include <arancini/txlat/txlat-engine.h>
#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace arancini::txlat;
using namespace arancini::elf;
using namespace arancini::ir;
using namespace arancini::input;
using namespace arancini::input::x86;
using namespace arancini::output;
using namespace arancini::output::llvm;

static std::set<std::string> allowed_symbols = { "_start", "test", "__libc_start_main", "_dl_aux_init", "__assert_fail", "__dcgettext", "__dcigettext" };

static std::map<std::string, std::function<std::unique_ptr<arancini::output::output_engine>()>> translation_engines
	= { { "llvm", [] { return std::make_unique<arancini::output::llvm::llvm_output_engine>(); } } };

void txlat_engine::process_options(arancini::output::output_engine &oe, const boost::program_options::variables_map &cmdline)
{
	if (auto llvmoe = dynamic_cast<arancini::output::llvm::llvm_output_engine *>(&oe)) {
		llvmoe->set_debug(cmdline.count("debug"));
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
	// Parse the input ELF file
	elf_reader elf(cmdline.at("input").as<std::string>());
	elf.parse();

	// TODO: Figure the input engine out from ELF architecture header
	auto ia = std::make_unique<arancini::input::x86::x86_input_arch>();

	// Figure out the output engine
	auto requested_engine = cmdline.at("engine").as<std::string>();
	auto engine_factory = translation_engines.find(requested_engine);
	if (engine_factory == translation_engines.end()) {
		std::cerr << "Error: unknown translation engine '" << requested_engine << "'" << std::endl;
		std::cerr << "Available engines:" << std::endl;
		for (const auto &e : translation_engines) {
			std::cerr << "  " << e.first << std::endl;
		}

		throw std::runtime_error("Unknown translation engine");
	}

	// Invoke the factory to construct the output engine
	auto oe = engine_factory->second();
	process_options(*oe, cmdline);

	// Loop over each symbol table, and translate the symbol.
	for (auto s : elf.sections()) {
		if (s->type() == section_type::symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			for (const auto &sym : st->symbols()) {
				// std::cerr << "Symbol: " << sym.name() << "\n";
				if (allowed_symbols.count(sym.name())) {
					oe->add_chunk(translate_symbol(*ia, elf, sym));
				}
			}
		}
	}

	if (cmdline.count("debug")) {
                debug_visitor dv(std::cout);
		for (auto c : oe->chunks()) {
			c->accept(dv);
		}
	}
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

	// Invoke the output engine.
	static_output_personality sop("static-translation.o");
	oe->generate(sop);

	// TODO: Generate loadable sections

	// Do the link - TODO: this is awful.

	std::string cmd = "g++ -o " + cmdline.at("output").as<std::string>() + " -no-pie static-translation.o -L out -larancini-runtime";
	std::system(cmd.c_str());
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

	auto start = std::chrono::high_resolution_clock::now();
	auto cv = ia.translate_chunk(sym.value(), symbol_data, sym.size(), false);
	auto dur = std::chrono::high_resolution_clock::now() - start;

	std::cerr << "symbol translation time: " << std::dec << std::chrono::duration_cast<std::chrono::microseconds>(dur).count() << " us" << std::endl;
	return cv;
}
