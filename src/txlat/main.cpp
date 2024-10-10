#include <arancini/txlat/txlat-engine.h>
#include <arancini/util/logger.h>
#include <boost/program_options.hpp>

#include <iostream>
#include <optional>
#include <filesystem>

using namespace arancini::txlat;
namespace po = boost::program_options;

static std::optional<po::variables_map> init_options(int argc, const char *argv[]) {
    const char* flag = getenv("ARANCINI_ENABLE_LOG");
    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            util::global_logger.enable(true);
        else if (util::case_ignore_string_equal(flag, "false"))
            util::global_logger.enable(false);
        else throw std::runtime_error("ARANCINI_ENABLE_LOG must be set to either true or false");
    }

    // Determine logger level
    flag = getenv("ARANCINI_LOG_LEVEL");
    if (flag && util::global_logger.is_enabled()) {
        if (util::case_ignore_string_equal(flag, "debug"))
            util::global_logger.set_level(util::global_logging::levels::debug);
        else if (util::case_ignore_string_equal(flag, "info"))
            util::global_logger.set_level(util::global_logging::levels::info);
        else if (util::case_ignore_string_equal(flag, "warn"))
            util::global_logger.set_level(util::global_logging::levels::warn);
        else if (util::case_ignore_string_equal(flag, "error"))
            util::global_logger.set_level(util::global_logging::levels::error);
        else if (util::case_ignore_string_equal(flag, "fatal"))
            util::global_logger.set_level(util::global_logging::levels::fatal);
        else throw std::runtime_error("ARANCINI_LOG_LEVEL must be set to one among: debug, info, warn, error or fatal (case-insensitive)");
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log level; setting log level to default [info]\n";
        util::global_logger.set_level(util::global_logging::levels::info);
    }

	po::options_description desc("Command-line options");

	desc.add_options() //
		("help,h", "Displays usage information") //
		("input,I", po::value<std::string>()->required(), "The ELF file that is being translated") //
		("output,O", po::value<std::string>(), "The output file that is generated (omit if you don't want to produce a translated binary)") //
		("library,l", po::value<std::vector<std::string>>(),
			"Translated versions of the libraries this binary depends on (relative or absolute path including filename)") //
		("syntax", po::value<std::string>()->default_value("intel"),
			"Specify the syntax to use when disassembling host instructions (x86 input only: att or intel)") //
		("graph", po::value<std::filesystem::path>(), "Creates a DOT graph file representing the input ELF translation") //
		("no-static", "Do not do any static translation") //
#ifndef CROSS_TRANSLATE
		("runtime-lib-path", po::value<std::filesystem::path>()->default_value(ARANCINI_LIBPATH), "Path to arancini libraries (defaults specified by build system)") //
		("static-binary", po::value<std::string>()->implicit_value(ARANCINI_LIBDIR)->zero_tokens(),
			"Link the generated binary statically to the arancini libraries inside this path. Requires to have built the arancini-runtime-static target. "
			"Default specified by build system.") //
		("cxx-compiler-path", po::value<std::string>()->default_value("g++"), "Path to C++ compiler to use for translated binary") //
#else
		("runtime-lib-path", po::value<std::string>()->required(),
			"Path to arancini libraries. This txlat is configured to cross translate, so you are"
			" required to provide the path of the arancini-runtime library compiled for " DBT_ARCH_STR ". ") //
		("static-binary", po::value<std::string>(),
			"Link the generated binary statically to the arancini libraries inside this path. Requires to have built the arancini-runtime-static target "
			"for " DBT_ARCH_STR ". ") //
		("cxx-compiler-path", po::value<std::string>()->required(),
			"Path to C++ compiler to use for translated binary. This txlat is configured to cross translate, so you are required to provide a valid "
			"cross-compiler for " DBT_ARCH_STR ". ") //
#endif
		("wrapper", po::value<std::string>()) //
		("debug-gen", "Include debugging information in the generated output binary") //
		("debug", "Enable debugging output") //
		("verbose-link", "Enable verbose output of the linker (-Wl,--verbose)") //
		("no-script", "Do not use a linker script. Also does not include any data from the input binary. Mainly useful as a step to generate a linker script.") //
		("dump-llvm", po::value<std::string>(), "Dump generated LLVM IR") //
		("keep-objs", po::value<std::string>(), "Keep generated obj files in <prefix> for manual linking") //
		("nlib", po::value<std::string>(), "Parse the file at the given path for native library method definitions to substitute when translating.") //
        ("disable-flag-opt", "Disable optimizations that eliminate uneeded flag computations") //
		("llvm-codegen-nofence", "Do not generate fences on memory accesses. Only safe for single-threaded applications.");

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
        if (::util::global_logger.get_level() <= ::util::global_logging::levels::error)
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

