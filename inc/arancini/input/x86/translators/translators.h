#pragma once

#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <cstdlib>
#include <memory>
#include <map>

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

  enum class translation_result { normal, noop, end_of_block, fail };

  class translator {
  public:
    translator(ir_builder &builder)
      : builder_(builder)
    {
    }

    translation_result translate(off_t address, xed_decoded_inst_t *xed_inst, const std::string& disasm);

  protected:
    virtual void do_translate() = 0;

    ir_builder &builder() const { return builder_; }

    xed_decoded_inst_t *xed_inst() const { return xed_inst_; }

    action_node *write_operand(int opnum, port &value);
    value_node *read_operand(int opnum);
    ssize_t get_operand_width(int opnum);
    bool is_memory_operand(int opnum);
    value_node *compute_address(int mem_idx);

    enum class reg_offsets {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
    };

#define COUNTER_BASE __COUNTER__
    std::map<unsigned long, unsigned long> off_to_idx {
#define DEFREG(ctype, ltype, name) {X86_OFFSET_OF(name), __COUNTER__ - COUNTER_BASE - 1},
#include <arancini/input/x86/reg.def>
#undef DEFREG
    };

    std::map<unsigned long, const char *> off_to_name {
#define DEFREG(ctype, ltype, name) { X86_OFFSET_OF(name), #name },
#include <arancini/input/x86/reg.def>
#undef DEFREG
	};

    action_node *write_reg(reg_offsets reg, port & value);
    value_node *read_reg(const value_type &vt, reg_offsets reg);

    /// @brief Convert a register name from XED to an arancini register
    ///        Should only be used in helper functions, not in instruction translation directly. Use a reg_offsets value directly.
    ///        Only use if you cannot do so, e.g., for EAX if it is not exposed as an operand by XED (unlikely)
    reg_offsets xedreg_to_offset(xed_reg_enum_t reg);
    unsigned long offset_to_idx(reg_offsets reg);
    const char *offset_to_name(reg_offsets reg);

    enum flag_op {
      ignore,
      set0,
      set1,
      update
    };
		/// @brief Write flag register with a flag_op (ignore, set0, set1, update)
		/// @param op node affecting the flags
		/// @param zf zero flag (ZF)
		/// @param cf carry flag (CF)
		/// @param of overflow floag (OF)
		/// @param sf sign flag (SF)
		/// @param pf parity flag (PF)
		/// @param af adjust flag (AF)
		void write_flags(value_node * op, flag_op zf, flag_op cf, flag_op of, flag_op sf, flag_op pf, flag_op af);

		enum class cond_type { nbe, nb, b, be, z, nle, nl, l, le, nz, no, np, ns, o, p, s };
		value_node *compute_cond(cond_type ct);

		value_node *auto_cast(const value_type &target_type, value_node *v);

		value_type type_of_operand(int opnum);

    /// @brief Dump the xed operand encoding of the instruction currently being translated
    ///        Use this function when discovering which operands to use when adding support for instructions
    void dump_xed_encoding(void);

	private:
		ir_builder &builder_;
		xed_decoded_inst_t *xed_inst_;
	};

#define DEFINE_TRANSLATOR(name)                 \
	class name##_translator : public translator { \
	public:                                       \
  name##_translator(ir_builder &builder)        \
  : translator(builder)                         \
		{                                           \
		}                                           \
                                                \
	protected:                                    \
  virtual void do_translate() override;         \
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
} // namespace arancini::input::x86::translators
