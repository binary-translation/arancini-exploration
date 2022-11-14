#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace arancini::output::dynamic::x86 {
class x86_output_engine_impl;

class x86_output_engine : public output_engine {
public:
	x86_output_engine();
	~x86_output_engine();

	void generate(const output_personality &personality) override;

private:
	std::unique_ptr<x86_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::x86
