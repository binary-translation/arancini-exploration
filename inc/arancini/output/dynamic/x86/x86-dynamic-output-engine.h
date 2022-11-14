#pragma once

#include <arancini/output/dynamic/dynamic-output-engine.h>
#include <memory>

namespace arancini::output::dynamic::x86 {
class x86_dynamic_output_engine_impl;

class x86_dynamic_output_engine : public dynamic_output_engine {
public:
	x86_dynamic_output_engine();
	~x86_dynamic_output_engine();

	void lower(ir::node *node, machine_code_writer &writer) override;

private:
	std::unique_ptr<x86_dynamic_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::x86
