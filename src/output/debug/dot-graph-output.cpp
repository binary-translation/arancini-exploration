#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/debug/dot-graph-output.h>
#include <arancini/output/output-personality.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace arancini::output::debug;
using namespace arancini::ir;

void dot_graph_output::generate(const output_personality &personality)
{
	if (personality.kind() != output_personality_kind::personality_static) {
		throw std::runtime_error("dot graph output requires static personality output");
	}

	auto output_file = ((const static_output_personality &)personality).output_file();

	std::ostream *os;
	if (output_file == "-") {
		os = &std::cout;
	} else {
		os = new std::ofstream(output_file);
	}

	dot_graph_generator g(*os);

	for (auto c : chunks()) {
		c->accept(g);
	}

	if (os != &std::cout) {
		delete os;
	}
}
