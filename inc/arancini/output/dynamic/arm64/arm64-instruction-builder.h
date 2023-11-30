#pragma once

#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace arancini::output::dynamic::arm64 {

template <typename T, typename A, typename B> using is_one_of = std::disjunction<std::is_same<T, A>, std::is_same<T, B>>;

template <typename T> using is_reg = std::enable_if_t<is_one_of<T, preg_operand, vreg_operand>::value, int>;

template <typename T> using is_imm = std::enable_if_t<std::is_same<T, immediate_operand>::value, int>;

template <typename T>
using is_reg_or_immediate = std::enable_if_t<std::disjunction<is_one_of<T, preg_operand, vreg_operand>, std::is_same<T, immediate_operand>>::value, int>;

class instruction_builder {
public:
#define ARITH_OP_BASIC(name)                                                                                                                                   \
	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>                                              \
	void name(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")                                                                  \
	{                                                                                                                                                          \
		append(instruction(#name, def(keep(dst)), use(src1), use(src2)).add_comment(comment));                                                                 \
	}

#define ARITH_OP_SHIFT(name)                                                                                                                                   \
	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>                                              \
	void name(const T1 &dst, const T2 &src1, const T3 &src2, const shift_operand &shift)                                                                       \
	{                                                                                                                                                          \
		append(instruction(#name, def(keep(dst)), use(src1), use(src2), use(shift)));                                                                          \
	}

// TODO: refactor everything this way
#define ARITH_OP(name)                                                                                                                                         \
	ARITH_OP_BASIC(name)                                                                                                                                       \
	ARITH_OP_SHIFT(name)

	// ADD
	ARITH_OP(add);

	// ADDS
	ARITH_OP(adds);

	// ADCS
	ARITH_OP(adcs);

	// SUB
	ARITH_OP(sub);

	// SUBS
	ARITH_OP(subs);

	// SBCS
	ARITH_OP(sbcs);

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void orr_(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("orr", def(dst), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void and_(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("and", def(keep(dst)), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void ands(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("ands", def(keep(dst)), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void eor_(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("eor", def(dst), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg_or_immediate<T2> = 0> void not_(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("mvn", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void neg(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("neg", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void movn(const T1 &dst, const immediate_operand &src, const shift_operand &shift, const std::string &comment = "")
	{
		append(instruction("movn", def(dst), use(src), use(shift)).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void movz(const T1 &dst, const immediate_operand &src, const shift_operand &shift, const std::string &comment = "")
	{
		append(instruction("movz", def(dst), use(src), use(shift)).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void movk(const T1 &dst, const immediate_operand &src, const shift_operand &shift, const std::string &comment = "")
	{
		append(instruction("movk", usedef(dst), use(src), use(shift)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg_or_immediate<T2> = 0> void mov(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("mov", def(dst), use(src)).add_comment(comment));
	}

	void b(const label_operand &dest, const std::string &comment = "") { append(instruction("b", use(dest)).add_comment(comment).set_branch(true)); }

	void beq(const label_operand &dest, const std::string &comment = "") { append(instruction("beq", use(dest)).add_comment(comment).set_branch(true)); }

	void bl(const label_operand &dest, const std::string &comment = "") { append(instruction("bl", use(dest)).add_comment(comment).set_branch(true)); }

	void bne(const label_operand &dest, const std::string &comment = "") { append(instruction("bne", use(dest)).add_comment(comment).set_branch(true)); }

	template <typename T1, is_reg<T1> = 0> void cbz(const T1 &dest, const label_operand &label, const std::string &comment = "")
	{
		append(instruction("cbz", def(keep(dest)), use(label)).add_comment(comment).set_branch(true));
	}

	// TODO: check if this allocated correctly
	template <typename T1, is_reg<T1> = 0> void cbnz(const T1 &rt, const label_operand &dest, const std::string &comment = "")
	{
		append(instruction("cbnz", use(rt), use(dest)).add_comment(comment).set_branch(true));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg_or_immediate<T2> = 0> void cmn(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("cmn", usedef(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg_or_immediate<T2> = 0> void cmp(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("cmp", usedef(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg_or_immediate<T2> = 0> void tst(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("tst", usedef(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void lsl(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("lsl", def(dst), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void lsr(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("lsr", def(dst), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void asr(const T1 &dst, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("asr", def(dst), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void csel(const T1 &dst, const T2 &src1, const T3 &src2, const cond_operand &cond, const std::string &comment = "")
	{
		append(instruction("csel", def(dst), use(src1), use(src2), use(cond)).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void cset(const T1 &dst, const cond_operand &cond, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), use(cond)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0>
	void bfxil(const T1 &dst, const T2 &src1, const immediate_operand &lsb, const immediate_operand &width, const std::string &comment = "")
	{
		append(instruction("bfxil", usedef(dst), use(src1), use(lsb), use(width)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0>
	void bfi(const T1 &dst, const T2 &src1, const immediate_operand &immr, const immediate_operand &imms, const std::string &comment = "")
	{
		append(instruction("bfi", usedef(keep(dst)), use(src1), use(immr), use(imms)).add_comment(comment));
	}

#define LDR_VARIANTS(name)                                                                                                                                     \
	template <typename T1, is_reg<T1> = 0> void name(const T1 &dest, const memory_operand &base, const std::string &comment = "")                              \
	{                                                                                                                                                          \
		append(instruction(#name, def(dest), use(base)).add_comment(comment));                                                                                 \
	}

#define STR_VARIANTS(name)                                                                                                                                     \
	template <typename T1, is_reg<T1> = 0> void name(const T1 &src, const memory_operand &base, const std::string &comment = "")                               \
	{                                                                                                                                                          \
		append(instruction(#name, use(src), use(base)).add_comment(comment));                                                                                  \
	}

	LDR_VARIANTS(ldr);
	LDR_VARIANTS(ldrh);
	LDR_VARIANTS(ldrb);

	STR_VARIANTS(str);
	STR_VARIANTS(strh);
	STR_VARIANTS(strb);

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void mul(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("mul", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void smulh(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("smulh", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void smull(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("smull", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void umulh(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("umulh", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void umull(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("umull", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void fmul(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("fmul", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
	void sdiv(const T1 &dest, const T2 &src1, const T3 &src2, const std::string &comment = "")
	{
		append(instruction("sdiv", def(dest), use(src1), use(src2)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void fcvtzs(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("fcvtzs", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void fcvtzu(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("fcvtzu", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void fcvtas(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("fcvtas", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void fcvtau(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("fcvtau", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void scvtf(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("scvtf", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void ucvtf(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("ucvtf", def(dest), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void mrs(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("mrs", usedef(dest), usedef(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void msr(const T1 &dest, const T2 &src, const std::string &comment = "")
	{
		append(instruction("msr", usedef(dest), usedef(src)).add_comment(comment));
	}

	void ret(const std::string &comment = "") { append(instruction("ret").add_comment(comment)); }

	void brk(const immediate_operand &imm, const std::string &comment = "") { append(instruction("brk", use(imm)).add_comment(comment)); }

	void label(const std::string &label, const std::string &comment = "") { append(instruction(label_operand(label + ":")).add_comment(comment)); }

	template <typename T1, is_reg<T1> = 0> void setz(const T1 &dst, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), cond_operand("eq")).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void sets(const T1 &dst, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), cond_operand("lt")).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void setc(const T1 &dst, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), cond_operand("cs")).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void setcc(const T1 &dst, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), cond_operand("cc")).add_comment(comment));
	}

	template <typename T1, is_reg<T1> = 0> void seto(const T1 &dst, const std::string &comment = "")
	{
		append(instruction("cset", def(dst), cond_operand("vs")).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void sxtb(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("sxtb", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void sxth(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("sxth", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void sxtw(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("sxtw", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void uxtb(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("uxtb", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void uxth(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("uxth", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void uxtw(const T1 &dst, const T2 &src, const std::string &comment = "")
	{
		append(instruction("uxtw", def(dst), use(src)).add_comment(comment));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0>
	void cas(const T1 &dst, const T2 &src, const memory_operand &mem_addr, const std::string &comment = "")
	{
		append(instruction("cas", use(keep(dst)), use(src), use(mem_addr)).add_comment(comment));
	}

// ATOMICs
// LDXR{size} {Rt}, [Rn]
#define LD_A_XR(name, size)                                                                                                                                    \
	template <typename T1, is_reg<T1> = 0> void name##size(const T1 &dst, const memory_operand &mem, const std::string &comment = "")                          \
	{                                                                                                                                                          \
		append(instruction(#name #size, def(dst), use(mem)).add_comment(comment));                                                                             \
	}

#define ST_A_XR(name, size)                                                                                                                                    \
	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0>                                                                                        \
	void name##size(const T1 &status, const T2 &rt, const memory_operand &mem, const std::string &comment = "")                                                \
	{                                                                                                                                                          \
		append(instruction(#name #size, def(keep(status)), use(rt), use(mem)).add_comment(comment));                                                           \
	}

#define LD_A_XR_VARIANTS(name)                                                                                                                                 \
	LD_A_XR(name, b)                                                                                                                                           \
	LD_A_XR(name, h)                                                                                                                                           \
	LD_A_XR(name, w)                                                                                                                                           \
	LD_A_XR(name, )

#define ST_A_XR_VARIANTS(name)                                                                                                                                 \
	ST_A_XR(name, b)                                                                                                                                           \
	ST_A_XR(name, h)                                                                                                                                           \
	ST_A_XR(name, w)                                                                                                                                           \
	ST_A_XR(name, )

	LD_A_XR_VARIANTS(ldxr);
	LD_A_XR_VARIANTS(ldaxr);

	ST_A_XR_VARIANTS(stxr);
	ST_A_XR_VARIANTS(stlxr);

#define AMO_SIZE_VARIANT(name, suffix_type, suffix_size)                                                                                                       \
	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0>                                                                                        \
	void name##suffix_type##suffix_size(const T1 &rm, const T2 &rt, const memory_operand &mem)                                                                 \
	{                                                                                                                                                          \
		append(instruction(#name #suffix_type #suffix_size, use(rm), usedef(rt), use(mem)));                                                                   \
	}

#define AMO_SIZE_VARIANTS(name, size)                                                                                                                          \
	AMO_SIZE_VARIANT(name, , size)                                                                                                                             \
	AMO_SIZE_VARIANT(name, a, size)                                                                                                                            \
	AMO_SIZE_VARIANT(name, al, size)                                                                                                                           \
	AMO_SIZE_VARIANT(name, l, size)

#define AMO_SIZE_VARIANT_HW(name)                                                                                                                              \
	AMO_SIZE_VARIANTS(name, )                                                                                                                                  \
	AMO_SIZE_VARIANTS(name, h)                                                                                                                                 \
	AMO_SIZE_VARIANTS(name, w)

#define AMO_SIZE_VARIANT_BHW(name)                                                                                                                             \
	AMO_SIZE_VARIANT_HW(name)                                                                                                                                  \
	AMO_SIZE_VARIANTS(name, b)

	AMO_SIZE_VARIANT_BHW(swp);

	AMO_SIZE_VARIANT_BHW(ldadd);

	AMO_SIZE_VARIANT_BHW(ldclr);

	AMO_SIZE_VARIANT_BHW(ldeor);

	AMO_SIZE_VARIANT_BHW(ldset);

	AMO_SIZE_VARIANT_HW(ldsmax);

	AMO_SIZE_VARIANT_HW(ldsmin);

	AMO_SIZE_VARIANT_HW(ldumax);

	AMO_SIZE_VARIANT_HW(ldumin);

// NEON Vectors
// NOTE: these should be built only if there is no support for SVE/SVE2
//
// Otherwise, we should just use SVE2 operations directly
#define VOP_ARITH(name)                                                                                                                                        \
	template <typename T1, typename T2, typename T3, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>                                                           \
	void name(const std::string &size, const T1 &dest, const T2 &src1, const T3 &src2)                                                                         \
	{                                                                                                                                                          \
		append(instruction(#name "." + size, def(dest), use(src1), use(src2)));                                                                                \
	}

	// vector additions
	VOP_ARITH(vadd);
	VOP_ARITH(vqadd);
	VOP_ARITH(vhadd);
	VOP_ARITH(vrhadd);

	// vector subtractions
	VOP_ARITH(vsub);
	VOP_ARITH(vqsub);
	VOP_ARITH(vhsub);

	// vector multiplication
	// TODO: some not included
	VOP_ARITH(vmul);
	VOP_ARITH(vmla);
	VOP_ARITH(vmls);

	// vector absolute value
	VOP_ARITH(vabs);
	VOP_ARITH(vqabs);

	// vector negation
	VOP_ARITH(vneg);
	VOP_ARITH(vqneg);

	// vector round float
	VOP_ARITH(vrnd);
	VOP_ARITH(vrndi);
	VOP_ARITH(vrnda);

	// pairwise addition
	VOP_ARITH(vpadd);
	VOP_ARITH(vpaddl);

	// vector shifts
	// shift left, shift right, shift left long.
	VOP_ARITH(vshl);
	VOP_ARITH(vshr);
	VOP_ARITH(vshll);

	// vector comparisons
	// VCMP, VCEQ, VCGE, VCGT: compare, compare equal, compare greater than or equal, compare greater than.
	VOP_ARITH(vcmp);
	VOP_ARITH(vceq);
	VOP_ARITH(vcge);
	VOP_ARITH(vcgt);

	// min-max
	VOP_ARITH(vmin);
	VOP_ARITH(vmax);

	// reciprocal-sqrt
	VOP_ARITH(vrecpe);
	VOP_ARITH(vrsqrte);

	// SVE2
	// SVE2 version
	template <typename T1, typename T2, typename T3, typename T4, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0, is_reg<T4> = 0>
	void add(const T1 &dst, const T2 &pred, const T3 &src1, const T4 &src2)
	{
		append(instruction("add", def(dst), use(pred), use(src1), use(src2)));
	}

	// SVE2
	template <typename T1, typename T2, typename T3, typename T4, is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0, is_reg<T4> = 0>
	void sub(const T1 &dst, const T2 &pred, const T3 &src1, const T4 &src2)
	{
		append(instruction("sub", def(dst), use(pred), use(src1), use(src2)));
	}

	// SVE2
	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void ld1(const T1 &src, const T2 &pred, const memory_operand &addr)
	{
		append(instruction("ld1", use(src), use(pred), use(addr)));
	}

	template <typename T1, typename T2, is_reg<T1> = 0, is_reg<T2> = 0> void st1(const T1 &dest, const T2 &pred, const memory_operand &addr)
	{
		append(instruction("st1", def(dest), use(pred), use(addr)));
	}

	template <typename T1, is_reg<T1> = 0> void ptrue(const T1 &dest) { append(instruction("ptrue", def(dest))); }

	void insert_sep(const std::string &sep) { label(sep); }

	void insert_comment(const std::string &comment) { append(instruction("// " + comment)); }

	bool has_label(const std::string &label)
	{
		auto label_str = label + ":";
		auto insn = instructions_;
		return std::any_of(insn.rbegin(), insn.rend(), [&](const instruction &i) { return i.opcode() == label_str; });
	}

	void allocate();

	void emit(machine_code_writer &writer);

	void dump(std::ostream &os) const;

	size_t nr_instructions() const { return instructions_.size(); }

private:
	std::vector<instruction> instructions_;

	void append(const instruction &i) { instructions_.push_back(i); }

	void spill();
};
} // namespace arancini::output::dynamic::arm64
