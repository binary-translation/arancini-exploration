#pragma once

#include <arancini/ir/node.h>
#include <cstdlib>
#include <memory>
#include <string>

namespace arancini::ir {
class node;
}

namespace arancini::output::dynamic {
class machine_code_writer;

class translation_context {
  public:
    translation_context(machine_code_writer &writer) : writer_(writer) {}

    virtual ~translation_context() = default;

    virtual void begin_block() = 0;
    virtual void begin_instruction(off_t address,
                                   const std::string &disasm) = 0;
    virtual void end_instruction() = 0;
    virtual void end_block() = 0;

    virtual void chain(uint64_t chain_address, void *chain_target) {
        // Default to No-op
    };

    virtual void lower(const std::shared_ptr<ir::action_node> &n) = 0;

    machine_code_writer &writer() const { return writer_; }

  private:
    machine_code_writer &writer_;
};
} // namespace arancini::output::dynamic
