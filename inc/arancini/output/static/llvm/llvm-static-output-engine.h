#pragma once

#include <arancini/output/static/static-output-engine.h>
#include <memory>
#include <optional>

namespace arancini::output::o_static::llvm {
class llvm_static_output_engine_impl;

class llvm_static_output_engine : public static_output_engine {
	friend class llvm_static_output_engine_impl;

public:
	llvm_static_output_engine(const std::string &output_filename, const bool is_exec);
	virtual ~llvm_static_output_engine();

	void generate() override;

	void set_debug(bool dbg) { dbg_ = dbg; }

	void set_debug_dump_filename(std::string filename) { debug_dump_filename = filename; }

	bool is_exec() const { return is_exec_; };
private:
	std::unique_ptr<llvm_static_output_engine_impl> oei_;
	bool dbg_;
	const bool is_exec_;
	std::optional<std::string> debug_dump_filename;
};
} // namespace arancini::output::o_static::llvm
