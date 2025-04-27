#pragma once

#include <memory>

namespace arancini::output::dynamic {
class machine_code_writer;
class translation_context;

class dynamic_output_engine {
  public:
    virtual std::shared_ptr<translation_context>
    create_translation_context(machine_code_writer &writer) = 0;
};
} // namespace arancini::output::dynamic
