#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace arancini::output::llvm {
class llvm_output_engine_impl;

class llvm_output_engine : public output_engine {
	friend class llvm_output_engine_impl;

public:
	llvm_output_engine();
	~llvm_output_engine();

	void generate(const output_personality &personality) override;

	void set_debug(bool dbg) { dbg_ = dbg; }

private:
	std::unique_ptr<llvm_output_engine_impl> oei_;
	bool dbg_;
};
} // namespace arancini::output::llvm
