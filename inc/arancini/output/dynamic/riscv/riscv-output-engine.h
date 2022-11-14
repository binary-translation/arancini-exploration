#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace arancini::output::dynamic::riscv {
class riscv_output_engine_impl;

class riscv_output_engine : public output_engine {
public:
	riscv_output_engine();
	~riscv_output_engine();

	void generate(const output_personality &personality) override;

private:
	std::unique_ptr<riscv_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::riscv
