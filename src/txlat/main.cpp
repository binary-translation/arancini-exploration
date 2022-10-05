#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/output/debug/dot-graph-output.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <arancini/txlat/txlat-engine.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>

using namespace arancini::txlat;
namespace po = boost::program_options;

static std::optional<po::variables_map> init_options(int argc, const char *argv[])
{
	po::options_description desc("Command-line options");

	desc.add_options()("help,h", "Displays usage information")("input", po::value<std::string>()->required(), "The ELF file that is being translated")("output",
		po::value<std::string>()->required(),
		"The output file that is generated")("engine", po::value<std::string>()->default_value("llvm"), "The engine to use for translation");

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

static std::map<std::string, std::function<std::unique_ptr<arancini::output::output_engine>()>> translation_engines = {
	{ "llvm", [] { return std::make_unique<arancini::output::llvm::llvm_output_engine>(); } },
	{ "dot", [] { return std::make_unique<arancini::output::debug::dot_graph_output>(); } },
};

int main(int argc, const char *argv[])
{
	auto cmdline = init_options(argc, argv);
	if (!cmdline.has_value()) {
		return 1;
	}

	auto ia = std::make_unique<arancini::input::x86::x86_input_arch>();

	auto requested_engine = cmdline->at("engine").as<std::string>();
	auto engine_factory = translation_engines.find(requested_engine);
	if (engine_factory == translation_engines.end()) {
		std::cerr << "Error: unknown translation engine '" << requested_engine << "'" << std::endl;
		std::cerr << "Available engines:" << std::endl;
		for (const auto &e : translation_engines) {
			std::cerr << "  " << e.first << std::endl;
		}
		return 1;
	}

	auto oe = engine_factory->second();
	txlat_engine e(std::move(ia), std::move(oe));

	try {
		e.translate(cmdline->at("input").as<std::string>(), cmdline->at("output").as<std::string>());
	} catch (const std::exception &e) {
		std::cerr << "translation error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
