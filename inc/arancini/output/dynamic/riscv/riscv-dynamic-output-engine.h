#pragma once

#include <arancini/output/dynamic/dynamic-output-engine.h>
#include <memory>

namespace arancini::output::dynamic::riscv {
class riscv_dynamic_output_engine_impl;

class riscv_dynamic_output_engine : public dynamic_output_engine {
public:
	riscv_dynamic_output_engine();
	~riscv_dynamic_output_engine();

	void generate() override;

private:
	std::unique_ptr<riscv_dynamic_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::riscv
