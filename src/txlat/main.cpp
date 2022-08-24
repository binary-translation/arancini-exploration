#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/output/debug/dot-graph-output.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <arancini/txlat/txlat-engine.h>
#include <iostream>

using namespace arancini::txlat;

int main(int argc, const char *argv[])
{
	if (argc < 2) {
		std::cerr << "error: usage: " << argv[0] << " <input elf file> [--llvm]" << std::endl;
		return 1;
	}

	auto ia = std::make_unique<arancini::input::x86::x86_input_arch>();
	std::unique_ptr<arancini::output::output_engine> oe;

	for (int i = 2; i < argc; i++) {
		if (std::string(argv[i]) == "--llvm") {
			oe = std::make_unique<arancini::output::llvm::llvm_output_engine>();
		}
	}

	if (!oe) {
		oe = std::make_unique<arancini::output::debug::dot_graph_output>();
	}

	txlat_engine e(std::move(ia), std::move(oe));

	try {
		e.translate(argv[1]);
	} catch (const std::exception &e) {
		std::cerr << "translation error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
