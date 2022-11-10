#include <arancini/txlat/txlat-engine.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>

using namespace arancini::txlat;
namespace po = boost::program_options;

static std::optional<po::variables_map> init_options(int argc, const char *argv[])
{
	po::options_description desc("Command-line options");

	desc.add_options() //
		("help,h", "Displays usage information") //
		("input,I", po::value<std::string>()->required(), "The ELF file that is being translated") //
		("output,O", po::value<std::string>()->required(), "The output file that is generated") //
		("engine,E", po::value<std::string>()->default_value("llvm"), "The engine to use for translation") //
		("syntax", po::value<std::string>()->default_value("intel"),
			"Specify the syntax to use when disassembling host instructions (x86 input only: att or intel)") //
		("graph", po::value<std::string>(), "Creates a DOT graph file representing the input ELF translation") //
		("no-static", "Do not do any static translation") //
		("debug", "Enable debugging output");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return std::nullopt;
	}

	try {
		po::notify(vm);
	} catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl << std::endl;
		std::cout << desc << std::endl;
		return std::nullopt;
	}

	return vm;
}

int main(int argc, const char *argv[])
{
	auto cmdline = init_options(argc, argv);
	if (!cmdline.has_value()) {
		return 1;
	}

	txlat_engine e;

	try {
		e.translate(cmdline.value());
	} catch (const std::exception &e) {
		std::cerr << "translation error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
