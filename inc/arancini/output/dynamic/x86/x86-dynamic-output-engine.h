#pragma once

#include <arancini/output/dynamic/dynamic-output-engine.h>

namespace arancini::output::dynamic::x86 {
class x86_dynamic_output_engine : public dynamic_output_engine {
  public:
    x86_dynamic_output_engine();

    virtual std::shared_ptr<translation_context>
    create_translation_context(machine_code_writer &writer) override;
};
} // namespace arancini::output::dynamic::x86
