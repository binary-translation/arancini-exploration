#pragma once

#include <arancini/output/output-engine.h>

namespace arancini::output::debug {
class dot_graph_output : public output_engine {
public:
	void generate(const output_personality &personality) override;
};
} // namespace arancini::output::debug
