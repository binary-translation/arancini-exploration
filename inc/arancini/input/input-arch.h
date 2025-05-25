#pragma once

#include <cstdlib>
#include <memory>
#include <vector>
namespace arancini::native_lib {
class nlib_function;
}
namespace arancini::ir {
class ir_builder;
class internal_function_resolver;
} // namespace arancini::ir

namespace arancini::input {
class input_arch {
  public:
    input_arch(bool debug = false) : debug_(debug) {}

    virtual ~input_arch() {}
    virtual void translate_chunk(ir::ir_builder &builder, off_t base_address,
                                 const void *code, size_t code_size,
                                 bool basic_block, const std::string &name) = 0;

    virtual void gen_wrapper(ir::ir_builder &builder,
                             const native_lib::nlib_function &func) = 0;

    bool debug() const { return debug_; }

    virtual ir::internal_function_resolver &
    get_internal_function_resolver() = 0;

  private:
    bool debug_;
};
} // namespace arancini::input
