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

namespace arancini::output {
class output_engine;
}

namespace arancini::txlat {
class txlat_engine {
public:
	txlat_engine(std::unique_ptr<input::input_arch> ia, std::unique_ptr<output::output_engine> oe)
		: ia_(std::move(ia))
		, oe_(std::move(oe))
	{
	}

	void translate(const std::string &source);

private:
	std::unique_ptr<input::input_arch> ia_;
	std::unique_ptr<output::output_engine> oe_;

	std::shared_ptr<ir::chunk> translate_symbol(elf::elf_reader &reader, const elf::symbol &sym);
};
} // namespace arancini::txlat
