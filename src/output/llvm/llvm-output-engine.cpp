#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/chunk.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <iostream>

using namespace arancini::output::llvm;
using namespace arancini::ir;

void llvm_output_engine::generate()
{
	dot_graph_generator g(std::cout);

	for (auto c : chunks()) {
		c->accept(g);
	}
}
