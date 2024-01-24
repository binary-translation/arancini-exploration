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
		("output,O", po::value<std::string>(), "The output file that is generated (omit if you don't want to produce a translated binary)") //
		("syntax", po::value<std::string>()->default_value("intel"),
			"Specify the syntax to use when disassembling host instructions (x86 input only: att or intel)") //
		("graph", po::value<std::string>(), "Creates a DOT graph file representing the input ELF translation") //
		("no-static", "Do not do any static translation") //
        ("runtime-lib-path", po::value<std::string>()->default_value(ARANCINI_LIBPATH), "Path to arancini libraries (defaults specified by build system)") //
		("static-binary", po::value<std::string>()->implicit_value(ARANCINI_LIBDIR)->zero_tokens(),
			"Link the generated binary statically to the arancini libraries inside this path. Requires to have built the arancini-runtime-static target. "
			"Default specified by build system.") //
		("wrapper", po::value<std::string>()) //
        ("cxx-compiler-path", po::value<std::string>()->default_value("g++"), "Path to C++ compiler to use for translated binary") //
        ("debug-gen", "Include debugging information in the generated output binary") //
		("debug", "Enable debugging output") //
		("reg-arg-promotion", "Enable reg-arg-promotion pass") //
		("dump-llvm", po::value<std::string>(), "Dump generated LLVM IR");

    po::variables_map vm;
	try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }

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
