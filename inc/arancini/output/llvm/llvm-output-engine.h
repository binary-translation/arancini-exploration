#pragma once

#include <arancini/output/output-engine.h>

namespace arancini::output::llvm {
class llvm_output_engine : public output_engine {
public:
	void generate() override;
};
} // namespace arancini::output::llvm
