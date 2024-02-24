#include <arancini/txlat/txlat-engine.h>
#include <arancini/util/logger.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>

using namespace arancini::txlat;
namespace po = boost::program_options;

static std::optional<po::variables_map> init_options(int argc, const char *argv[]) {
    const char* flag = getenv("ARANCINI_ENABLE_LOG");
    bool log_status = false;
    if (flag) {
        if (!strcmp(flag, "true")) {
            log_status = true;
        } else if (!strcmp(flag, "false")) {
            log_status = false;
        } else throw std::runtime_error("ARANCINI_ENABLE_LOG must be set to either true or false");
    }

    std::cerr << "Logger status: " << std::boolalpha << log_status << ":" << util::global_logger.enable(log_status) << '\n';

    // Determine logger level
    flag = getenv("ARANCINI_LOG_LEVEL");
    util::basic_logging::levels level = util::basic_logging::levels::info;
    if (flag && util::global_logger.is_enabled()) {
        if (!strcmp(flag, "debug"))
            level = util::basic_logging::levels::debug;
        else if (!strcmp(flag, "info"))
            level = util::basic_logging::levels::info;
        else if (!strcmp(flag, "warn"))
            level = util::basic_logging::levels::warn;
        else if (!strcmp(flag, "error"))
            level = util::basic_logging::levels::error;
        else if (!strcmp(flag, "fatal"))
            level = util::basic_logging::levels::fatal;
        else throw std::runtime_error("ARANCINI_LOG_LEVEL must be set to one among: debug, info, warn, error or fatal");
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log level; setting log level to default [info]\n";
    }

    // Set logger level
    util::global_logger.set_level(level);

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
		("debug", "Enable debugging output");

    po::variables_map vm;
	try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }

		po::notify(vm);
	} catch (std::exception &e) {
        ::util::global_logger.error("{}\n", e.what());
        if (::util::global_logger.get_level() <= ::util::basic_logging::levels::error)
            std::cerr << desc << '\n';
		return std::nullopt;
	}

	return vm;
}

int main(int argc, const char *argv[]) {
	auto cmdline = init_options(argc, argv);
	if (!cmdline.has_value()) {
		return 1;
	}

	txlat_engine e;

	try {
		e.translate(cmdline.value());
	} catch (const std::exception &e) {
        ::util::global_logger.error("translation error: {}\n", e.what());
		return 1;
	}

	return 0;
}

