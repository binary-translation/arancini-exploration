#pragma once

#include <boost/program_options.hpp>
#include <memory>
#include <string>

namespace arancini::elf {
class elf_reader;
class symbol;
class program_header;
} // namespace arancini::elf

namespace arancini::ir {
class chunk;
}

namespace arancini::input {
class input_arch;
}

namespace arancini::output::o_static {
class static_output_engine;
}

namespace arancini::util {
class tempfile;
}

namespace arancini::txlat {
class txlat_engine {
public:
	void translate(const boost::program_options::variables_map &cmdline);

private:
	void process_options(arancini::output::o_static::static_output_engine &oe, const boost::program_options::variables_map &cmdline);
	std::shared_ptr<ir::chunk> translate_symbol(arancini::input::input_arch &ia, elf::elf_reader &reader, const elf::symbol &sym);
  void generate_dot_graph(arancini::output::o_static::static_output_engine &oe, std::string filename);
  void optimise(arancini::output::o_static::static_output_engine &oe, const boost::program_options::variables_map &cmdline);

	static void generate_guest_sections(const std::shared_ptr<util::tempfile> &phobjsrc, elf::elf_reader &elf,
		const std::vector<std::shared_ptr<elf::program_header>> &load_phdrs, const std::basic_string<char> &filename);
};
} // namespace arancini::txlat
