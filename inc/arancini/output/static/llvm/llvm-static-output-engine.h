#pragma once

#include <arancini/output/static/static-output-engine.h>
#include <memory>

namespace arancini::output::o_static::llvm {
class llvm_static_output_engine_impl;

class llvm_static_output_engine : public static_output_engine {
	friend class llvm_static_output_engine_impl;

public:
	llvm_static_output_engine(const std::string &output_filename);
	virtual ~llvm_static_output_engine();

	void generate() override;

	void set_debug(bool dbg) { dbg_ = dbg; }

private:
	std::unique_ptr<llvm_static_output_engine_impl> oei_;
	bool dbg_;
};
} // namespace arancini::output::o_static::llvm
