#pragma once

#include <arancini/output/dynamic/dynamic-output-engine.h>

namespace arancini::output::dynamic::arm64 {
class arm64_dynamic_output_engine : public dynamic_output_engine {
public:
	virtual std::shared_ptr<translation_context> create_translation_context(machine_code_writer &writer) override;
};
} // namespace arancini::output::dynamic::arm64
