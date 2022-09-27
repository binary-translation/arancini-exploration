#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/debug/dot-graph-output.h>
#include <iostream>

using namespace arancini::output::debug;
using namespace arancini::ir;

void dot_graph_output::generate(const output_personality &personality)
{
	dot_graph_generator g(std::cout);

	for (auto c : chunks()) {
		c->accept(g);
	}
}
