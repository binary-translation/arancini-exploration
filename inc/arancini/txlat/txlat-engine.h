#pragma once

#include <boost/program_options.hpp>
#include <memory>
#include <string>

namespace arancini::elf {
class elf_reader;
class symbol;
class symbol_table;
class program_header;
class rela_table;
class relr_array;
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
class basefile;
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
	static void add_symbol_to_output(const std::vector<std::shared_ptr<elf::program_header>> &phbins, const std::map<off_t, unsigned int> &end_addresses,
		const elf::symbol &sym, std::ofstream &s, std::map<uint64_t, std::string> &ifuncs, bool force_global = false, bool omit_prefix = false);

	static std::map<uint64_t, std::string> generate_guest_sections(const std::shared_ptr<util::basefile> &phobjsrc, elf::elf_reader &elf,
		const std::vector<std::shared_ptr<elf::program_header>> &load_phdrs, const std::basic_string<char> &filename,
		const std::shared_ptr<elf::symbol_table> &dyn_sym, const std::vector<std::shared_ptr<elf::rela_table>> &relocations,
		const std::vector<std::shared_ptr<elf::relr_array>> &relocations_r, const std::shared_ptr<elf::symbol_table> &sym_t,
		const std::vector<std::shared_ptr<elf::program_header>> &tls);
};
} // namespace arancini::txlat
