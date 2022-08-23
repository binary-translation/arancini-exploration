#include <arancini/elf/elf-reader.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/chunk.h>
#include <arancini/txlat/txlat-engine.h>
#include <iostream>

using namespace arancini::txlat;
using namespace arancini::elf;
using namespace arancini::ir;
using namespace arancini::input::x86;

void txlat_engine::translate(const std::string &input)
{
	elf_reader elf(input);
	elf.parse();

	for (auto s : elf.sections()) {
		if (s->type() == section_type::symbol_table) {
			auto st = std::static_pointer_cast<symbol_table>(s);
			for (const auto &sym : st->symbols()) {
				if (sym.name() == "_start") {
					translate_symbol(elf, sym);
				}
			}
		}
	}
}

void txlat_engine::translate_symbol(elf_reader &reader, const symbol &sym)
{
	//std::cerr << "translating symbol " << sym.name() << ", value=" << std::hex << sym.value() << ", size=" << sym.size() << ", section=" << sym.section_index()
	//		  << std::endl;

	x86_input_arch input_arch;

	auto section = reader.get_section(sym.section_index());
	if (!section) {
		throw std::runtime_error("unable to resolve symbol section");
	}

	off_t symbol_offset_in_section = sym.value() - section->address();

	const void *symbol_data = (const void *)((uintptr_t)section->data() + symbol_offset_in_section);

	auto c = input_arch.translate_chunk(sym.value(), symbol_data, sym.size());

	dot_graph_generator g(std::cout);
	c->accept(g);
}
