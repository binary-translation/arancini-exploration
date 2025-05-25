#pragma once

#include <arancini/input/registers.h>
#include <cstdlib>
#include <map>
#include <memory>

extern "C" {
#include <xed/xed-interface.h>
}

namespace arancini::ir {
class packet;
class port;
class action_node;
class value_node;
class value_type;
class ir_builder;
} // namespace arancini::ir

namespace arancini::input::x86::translators {

using namespace arancini::ir;
using namespace arancini::input::x86;

enum class translation_result { normal, noop, end_of_block, fail };

class translator {
  public:
    translator(ir_builder &builder) : builder_(builder) {}

    virtual ~translator() = default;

    translation_result translate(off_t address, xed_decoded_inst_t *xed_inst,
                                 const std::string &disasm);

  protected:
    virtual void do_translate() = 0;

    ir_builder &builder() const { return builder_; }

    xed_decoded_inst_t *xed_inst() const { return xed_inst_; }

    action_node *write_operand(int opnum, port &value);
    value_node *read_operand(int opnum);
    ssize_t get_operand_width(int opnum);
    bool is_memory_operand(int opnum);
    bool is_immediate_operand(int opnum);
    value_node *compute_address(int mem_idx);
    /// @brief Generates nodes to compute the address of an element on the FPU
    /// stack
    /// @param stack_idx: the index for which the address is queried, starting
    /// from top of stack
    /// @return a tree computing: x87_stack_base + (x87_stack_top_index * 10) +
    /// (stack_idx * 10)
    value_node *compute_fpu_stack_addr(int stack_idx);

    action_node *write_reg(reg_offsets reg, port &value);
    value_node *read_reg(const value_type &vt, reg_offsets reg);

    /// @brief Convert a register name from XED to an arancini register
    ///        Should only be used in helper functions, not in instruction
    ///        translation directly. Use a reg_offsets value directly. Only use
    ///        if you cannot do so, e.g., for EAX if it is not exposed as an
    ///        operand by XED (unlikely)
    reg_offsets xedreg_to_offset(xed_reg_enum_t reg);

    enum flag_op { ignore, set0, set1, update };
    /// @brief Write flag register with a flag_op (ignore, set0, set1, update)
    /// @param op node affecting the flags
    /// @param zf zero flag (ZF)
    /// @param cf carry flag (CF)
    /// @param of overflow floag (OF)
    /// @param sf sign flag (SF)
    /// @param pf parity flag (PF)
    /// @param af adjust flag (AF)
    void write_flags(value_node *op, flag_op zf, flag_op cf, flag_op of,
                     flag_op sf, flag_op pf, flag_op af);

    // x87 FPU stack manipulation

    /// @brief Get a value from the x87 FPU stack
    /// @param stack_idx index in the stack to get, starting from top of stack
    /// @return The value located at this index in the FPU stack
    value_node *fpu_stack_get(int stack_idx);

    /// @brief Set a value in the x87 FPU stack
    /// @param stack_idx index in the stack to set, starting from top of stack
    /// @param val value top write in the stack
    /// @return An action node representing the write to the stack
    action_node *fpu_stack_set(int stack_idx, port &val);

    /// @brief Move the x87 FPU stack index, updating the x87 tag register on
    /// the way.
    /// @param val the value added to the current top index (1 to pop, -1 to
    /// push, fail otherwise)
    /// @return the action node writng the new top index to the x87 status
    /// register
    action_node *fpu_stack_top_move(int val);

    enum class cond_type {
        nbe,
        nb,
        b,
        be,
        z,
        nle,
        nl,
        l,
        le,
        nz,
        no,
        np,
        ns,
        o,
        p,
        s
    };
    value_node *compute_cond(cond_type ct);

    value_node *auto_cast(const value_type &target_type, value_node *v);

    value_type type_of_operand(int opnum);

    /// @brief Dump the xed operand encoding of the instruction currently being
    /// translated
    ///        Use this function when discovering which operands to use when
    ///        adding support for instructions
    void dump_xed_encoding(void);

  private:
    ir_builder &builder_;
    xed_decoded_inst_t *xed_inst_;
};

#define DEFINE_TRANSLATOR(name)                                                \
    class name##_translator : public translator {                              \
      public:                                                                  \
        name##_translator(ir_builder &builder) : translator(builder) {}        \
                                                                               \
      protected:                                                               \
        virtual void do_translate() override;                                  \
    };

DEFINE_TRANSLATOR(mov)
DEFINE_TRANSLATOR(cmov)
DEFINE_TRANSLATOR(setcc)
DEFINE_TRANSLATOR(jcc)
DEFINE_TRANSLATOR(nop)
DEFINE_TRANSLATOR(unop)
DEFINE_TRANSLATOR(binop)
DEFINE_TRANSLATOR(stack)
DEFINE_TRANSLATOR(branch)
DEFINE_TRANSLATOR(shifts)
DEFINE_TRANSLATOR(muldiv)
DEFINE_TRANSLATOR(rep)
DEFINE_TRANSLATOR(punpck)
DEFINE_TRANSLATOR(fpvec)
DEFINE_TRANSLATOR(shuffle)
DEFINE_TRANSLATOR(atomic)
DEFINE_TRANSLATOR(fpu)
DEFINE_TRANSLATOR(control)
DEFINE_TRANSLATOR(interrupt)
DEFINE_TRANSLATOR(io)
DEFINE_TRANSLATOR(unimplemented)
} // namespace arancini::input::x86::translators
