#pragma once

#include <cstdlib>
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

enum class translation_result { normal, end_of_block, fail };

class translator {
public:
	translator(ir_builder &builder)
		: builder_(builder)
	{
	}

	translation_result translate(off_t address, xed_decoded_inst_t *xed_inst);

protected:
	virtual void do_translate() = 0;

	ir_builder &builder() const { return builder_; }

	xed_decoded_inst_t *xed_inst() const { return xed_inst_; }

	action_node *write_operand(int opnum, port &value);
	value_node *read_operand(int opnum);
	value_node *compute_address(int mem_idx);

	enum class reg_offsets { rip = 0, rax = 1, rcx = 2, rdx = 3, rbx = 4, zf = 17, cf = 18, of = 19, sf = 20, pf = 21, xmm0 = 22, fs = 38, gs = 39 };

	action_node *write_reg(reg_offsets reg, port &value);
	value_node *read_reg(const value_type &vt, reg_offsets reg);

	reg_offsets reg_to_offset(xed_reg_enum_t reg);

	enum flag_op { ignore, set0, set1, update };
	/// @brief Write flag register with a flag_op (ignore, set0, set1, update)
	/// @param op node affecting the flags
	/// @param zf zero flag (ZF)
	/// @param cf carry flag (CF)
	/// @param of overflow floag (OF)
	/// @param sf sign flag (SF)
	/// @param pf parity flag (PF)
	/// @param af adjust flag (AF)
	void write_flags(value_node *op, flag_op zf, flag_op cf, flag_op of, flag_op sf, flag_op pf, flag_op af);

	enum class cond_type { nbe, nb, b, be, z, nle, nl, l, le, nz, no, np, ns, o, p, s };
	value_node *compute_cond(cond_type ct);

	value_node *auto_cast(const value_type &target_type, value_node *v);

	value_type type_of_operand(int opnum);

private:
	ir_builder &builder_;
	xed_decoded_inst_t *xed_inst_;
};

#define DEFINE_TRANSLATOR(name)                                                                                                                                \
	class name##_translator : public translator {                                                                                                              \
	public:                                                                                                                                                    \
		name##_translator(ir_builder &builder)                                                                                                                 \
			: translator(builder)                                                                                                                              \
		{                                                                                                                                                      \
		}                                                                                                                                                      \
                                                                                                                                                               \
	protected:                                                                                                                                                 \
		virtual void do_translate() override;                                                                                                                  \
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
} // namespace arancini::input::x86::translators
