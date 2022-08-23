#pragma once

#include <memory>
#include <string>

namespace arancini::elf {
class elf_reader;
class symbol;
} // namespace arancini::elf

namespace arancini::ir {
class chunk;
}

namespace arancini::input {
class input_arch;
}

namespace arancini::txlat {
class txlat_engine {
public:
	void translate(const std::string &source);

private:
	std::shared_ptr<ir::chunk> translate_symbol(input::input_arch &in, elf::elf_reader &reader, const elf::symbol &sym);
};
} // namespace arancini::txlat
