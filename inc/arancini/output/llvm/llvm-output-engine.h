#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace arancini::output::llvm {
class llvm_output_engine_impl;

class llvm_output_engine : public output_engine {
public:
	llvm_output_engine();
	~llvm_output_engine();

	void generate(const output_personality &personality) override;

private:
	std::unique_ptr<llvm_output_engine_impl> oei_;
};
} // namespace arancini::output::llvm
