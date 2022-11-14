#pragma once

#include <arancini/output/dynamic/dynamic-output-engine.h>
#include <memory>

namespace arancini::output::dynamic::arm64 {
class arm64_dynamic_output_engine_impl;

class arm64_dynamic_output_engine : public dynamic_output_engine {
public:
	arm64_dynamic_output_engine();
	~arm64_dynamic_output_engine();

	void generate() override;

private:
	std::unique_ptr<arm64_dynamic_output_engine_impl> oei_;
};
} // namespace arancini::output::dynamic::arm64
