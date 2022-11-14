#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace arancini::output::dynamic::arm64 {
class arm64_output_engine_impl;

class arm64_output_engine : public output_engine {
public:
	arm64_output_engine();
	~arm64_output_engine();

	void generate(const output_personality &personality) override;

private:
	std::unique_ptr<arm64_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::arm64
