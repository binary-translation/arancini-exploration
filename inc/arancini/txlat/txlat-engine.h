#pragma once

#include <string>

namespace arancini::elf {
class elf_reader;
class symbol;
} // namespace arancini::elf

namespace arancini::txlat {
class txlat_engine {
public:
	void translate(const std::string &source);

private:
	void translate_symbol(elf::elf_reader &reader, const elf::symbol &sym);
};
} // namespace arancini::txlat
