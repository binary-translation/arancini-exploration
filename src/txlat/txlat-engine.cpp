#include <arancini/elf/elf-reader.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <arancini/txlat/txlat-engine.h>
#include <chrono>
#include <iostream>

using namespace arancini::txlat;
using namespace arancini::elf;
using namespace arancini::ir;
using namespace arancini::input;
using namespace arancini::input::x86;
using namespace arancini::output;
using namespace arancini::output::llvm;

static std::set<std::string> allowed_symbols = { "_start", "__libc_start_main", "_dl_aux_init", "__assert_fail", "__dcgettext", "__dcigettext" };



/*
This function acts as the main driver for the binary translation.
First it parses the ELF binary, lifting each section to the Arancini IR.
Finally, the lifted IR is processed by the output engine.
For example, the output engine can generate the target binary or a
visualisation of the Arancini IR.
*/
void txlat_engine::translate(const std::string &input)
{
	elf_reader elf(input);
	elf.parse();

	for (auto s : elf.sections()) {
		if (s->type() == section_type::symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			for (const auto &sym : st->symbols()) {
				if (allowed_symbols.count(sym.name())) {
					oe_->add_chunk(translate_symbol(elf, sym));
				}
			}
		}
	}

	oe_->generate();
}

/*
This function uses an x86 implementation of the input_architecture class to lift
x86 symbol sections to the Arancini IR.
*/
std::shared_ptr<chunk> txlat_engine::translate_symbol(elf_reader &reader, const symbol &sym)
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
	auto cv = ia_->translate_chunk(sym.value(), symbol_data, sym.size());
	auto dur = std::chrono::high_resolution_clock::now() - start;

	std::cerr << "symbol translation time: " << std::dec << std::chrono::duration_cast<std::chrono::microseconds>(dur).count() << " us" << std::endl;
	return cv;
}
