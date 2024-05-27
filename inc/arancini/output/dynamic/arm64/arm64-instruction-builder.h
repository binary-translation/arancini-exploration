#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <algorithm>
#include <type_traits>

namespace arancini::output::dynamic::arm64 {

template <typename T, typename A, typename B>
using is_one_of = std::disjunction<std::is_same<T, A>, std::is_same<T, B>>;

template <typename T>
using is_reg = std::enable_if_t<std::is_same_v<T, register_operand>, int>;

template <typename T>
using is_imm = std::enable_if_t<std::is_same_v<T, immediate_operand>, int>;

template <typename T>
using is_reg_or_immediate = std::enable_if_t<std::disjunction_v<std::is_same<T, register_operand>,
                                                              std::is_same<T, immediate_operand>>,
                                                              int>;

class instruction_builder {
public:
    using instruction_stream = typename std::vector<instruction>;

#define ARITH_OP_BASIC(name) \
    template <typename T1, typename T2, typename T3, \
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0> \
	void name(const T1 &dst, \
              const T2 &src1, \
              const T3 &src2, \
              std::string_view comment = "") { \
        append(instruction(#name, def(dst), use(src1), use(src2)).comment(comment).set_keep()); \
    }

#define ARITH_OP_SHIFT(name) \
    template <typename T1, typename T2, typename T3, \
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0> \
    void name(const T1 &dst, \
              const T2 &src1, \
              const T3 &src2, \
              const shift_operand &shift) { \
        append(instruction(#name, def(dst), use(src1), use(src2), use(shift)).set_keep()); \
    }

// TODO: refactor everything this way
#define ARITH_OP(name) \
    ARITH_OP_BASIC(name) \
    ARITH_OP_SHIFT(name) \

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

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void orr_(const T1 &dst,
              const T2 &src1,
              const T3 &src2, std::string_view comment = "") {
        append(instruction("orr", def(dst), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void and_(const T1 &dst,
              const T2 &src1,
              const T3 &src2, std::string_view comment = "") {
        append(instruction("and", def(dst), use(src1), use(src2)).set_keep().comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void ands(const T1 &dst,
              const T2 &src1,
              const T3 &src2, std::string_view comment = "") {
        append(instruction("ands", def(dst), use(src1), use(src2)).set_keep().comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void eor_(const T1 &dst,
              const T2 &src1,
              const T3 &src2, std::string_view comment = "") {
        append(instruction("eor", def(dst), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void not_(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("mvn", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void neg(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("neg", def(dst), use(src)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movn(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift, std::string_view comment = "") {
        append(instruction("movn", def(dst), use(src), use(shift)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movz(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift, std::string_view comment = "") {
        append(instruction("movz", def(dst), use(src), use(shift)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movk(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift, std::string_view comment = "") {
        append(instruction("movk", usedef(dst), use(src), use(shift)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void mov(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("mov", def(dst), use(src)).set_copy().comment(comment));
    }

    void b(const label_operand &dest, std::string_view comment = "") {
        append(instruction("b", use(dest)).comment(comment).set_branch());
    }

    void beq(const label_operand &dest, std::string_view comment = "") {
        append(instruction("beq", use(dest)).comment(comment));
    }

    void bl(const label_operand &dest, std::string_view comment = "") {
        append(instruction("bl", use(dest)).comment(comment));
    }

    void bne(const label_operand &dest, std::string_view comment = "") {
        append(instruction("bne", use(dest)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void cbz(const T1 &dest, const label_operand &label, std::string_view comment = "") {
        append(instruction("cbz", def(dest), use(label)).comment(comment).set_keep());
    }

    // TODO: check if this allocated correctly
    template <typename T1,
              is_reg<T1> = 0>
    void cbnz(const T1 &rt, const label_operand &dest, std::string_view comment = "") {
        append(instruction("cbnz", use(rt), use(dest)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void cmn(const T1 &dst,
             const T2 &src, std::string_view comment = "") {
        append(instruction("cmn", usedef(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void cmp(const T1 &dst,
             const T2 &src, std::string_view comment = "") {
        append(instruction("cmp", usedef(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void tst(const T1 &dst,
             const T2 &src, std::string_view comment = "") {
        append(instruction("tst", usedef(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void lsl(const T1 &dst,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("lsl", def(dst), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void lsr(const T1 &dst,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("lsr", def(dst), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void asr(const T1 &dst,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("asr", def(dst), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void csel(const T1 &dst,
              const T2 &src1,
              const T3 &src2,
              const conditional_operand &cond, std::string_view comment = "") {
        append(instruction("csel", def(dst), use(src1), use(src2), use(cond)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void cset(const T1 &dst,
              const conditional_operand &cond, std::string_view comment = "") {
        append(instruction("cset", def(dst), use(cond)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void bfxil(const T1 &dst,
             const T2 &src1,
             const immediate_operand &lsb,
             const immediate_operand &width, std::string_view comment = "") {
        append(instruction("bfxil", usedef(dst), use(src1), use(lsb), use(width)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void bfi(const T1 &dst,
             const T2 &src1,
             const immediate_operand &immr,
             const immediate_operand &imms, std::string_view comment = "") {
        append(instruction("bfi", usedef(dst), use(src1), use(immr), use(imms)).set_keep().comment(comment));
    }


#define LDR_VARIANTS(name) \
    template <typename T1, \
              is_reg<T1> = 0> \
    void name(const T1 &dest, \
             const memory_operand &base, std::string_view comment = "") { \
        append(instruction(#name, def(dest), use(base)).comment(comment)); \
    } \

#define STR_VARIANTS(name) \
    template <typename T1, \
              is_reg<T1> = 0> \
    void name(const T1 &src, \
             const memory_operand &base, std::string_view comment = "") { \
        append(instruction(#name, use(src), use(base)).comment(comment)); \
    } \

    LDR_VARIANTS(ldr);
    LDR_VARIANTS(ldrh);
    LDR_VARIANTS(ldrb);

    STR_VARIANTS(str);
    STR_VARIANTS(strh);
    STR_VARIANTS(strb);

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void mul(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("mul", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void smulh(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("smulh", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void smull(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("smull", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void umulh(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("umulh", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void umull(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("umull", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void fmul(const T1 &dest,
             const T2 &src1,
             const T3 &src2, std::string_view comment = "") {
        append(instruction("fmul", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void sdiv(const T1 &dest,
              const T2 &src1,
              const T3 &src2, std::string_view comment = "") {
        append(instruction("sdiv", def(dest), use(src1), use(src2)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtzs(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("fcvtzs", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtzu(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("fcvtzu", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtas(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("fcvtas", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtau(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("fcvtau", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void scvtf(const T1 &dest,
              const T2 &src, std::string_view comment = "") {
        append(instruction("scvtf", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void ucvtf(const T1 &dest,
              const T2 &src, std::string_view comment = "") {
        append(instruction("ucvtf", def(dest), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void mrs(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("mrs", usedef(dest), usedef(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void msr(const T1 &dest,
             const T2 &src, std::string_view comment = "") {
        append(instruction("msr", usedef(dest), usedef(src)).comment(comment));
    }

    void ret(std::string_view comment = "") {
        append(instruction("ret").comment(comment));
    }

    void brk(const immediate_operand &imm, std::string_view comment = "") {
        append(instruction("brk", use(imm)).comment(comment));
    }

    void label(std::string_view label, std::string_view comment = "") {
        append(instruction(fmt::format("{}:", label)).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setz(const T1 &dst, std::string_view comment = "") {
        append(instruction("cset", def(dst), conditional_operand("eq")).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void sets(const T1 &dst, std::string_view comment = "") {
        append(instruction("cset", def(dst), conditional_operand("lt")).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setc(const T1 &dst, std::string_view comment = "") {
        append(instruction("cset", def(dst), conditional_operand("cs")).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setcc(const T1 &dst, std::string_view comment = "") {
        append(instruction("cset", def(dst), conditional_operand("cc")).comment(comment));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void seto(const T1 &dst, std::string_view comment = "") {
        append(instruction("cset", def(dst), conditional_operand("vs")).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxtb(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("sxtb", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxth(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("sxth", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxtw(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("sxtw", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxtb(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("uxtb", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxth(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("uxth", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxtw(const T1 &dst, const T2 &src, std::string_view comment = "") {
        append(instruction("uxtw", def(dst), use(src)).comment(comment));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void cas(const T1 &dst, const T2 &src, const memory_operand &mem_addr, std::string_view comment = "") {
        append(instruction("cas", use(dst), use(src), use(mem_addr)).comment(comment).set_keep());
    }

// ATOMICs
    // LDXR{size} {Rt}, [Rn]
#define LD_A_XR(name, size) \
    template <typename T1, \
              is_reg<T1> = 0> \
    void name##size(const T1 &dst, const memory_operand &mem, std::string_view comment = "") { \
        append(instruction(#name#size, def(dst), use(mem)).comment(comment)); \
    }

#define ST_A_XR(name, size) \
    template <typename T1, typename T2, \
              is_reg<T1> = 0, is_reg<T2> = 0> \
    void name##size(const T1 &status, const T2 &rt, const memory_operand &mem, std::string_view comment = "") { \
        append(instruction(#name#size, def(status), use(rt), use(mem)).comment(comment).set_keep()); \
    }

#define LD_A_XR_VARIANTS(name) \
    LD_A_XR(name, b) \
    LD_A_XR(name, h) \
    LD_A_XR(name, w) \
    LD_A_XR(name,)

#define ST_A_XR_VARIANTS(name) \
    ST_A_XR(name, b) \
    ST_A_XR(name, h) \
    ST_A_XR(name, w) \
    ST_A_XR(name,)

    LD_A_XR_VARIANTS(ldxr);
    LD_A_XR_VARIANTS(ldaxr);

    ST_A_XR_VARIANTS(stxr);
    ST_A_XR_VARIANTS(stlxr);

#define AMO_SIZE_VARIANT(name, suffix_type, suffix_size) \
    template <typename T1, typename T2, \
              is_reg<T1> = 0, is_reg<T2> = 0> \
    void name##suffix_type##suffix_size(const T1 &rm, const T2 &rt, const memory_operand &mem) { \
        append(instruction(#name#suffix_type#suffix_size, use(rm), usedef(rt), use(mem))); \
    }

#define AMO_SIZE_VARIANTS(name, size) \
    AMO_SIZE_VARIANT(name, , size) \
    AMO_SIZE_VARIANT(name, a, size) \
    AMO_SIZE_VARIANT(name, al, size) \
    AMO_SIZE_VARIANT(name, l, size)

#define AMO_SIZE_VARIANT_HW(name) \
    AMO_SIZE_VARIANTS(name, ) \
    AMO_SIZE_VARIANTS(name, h) \
    AMO_SIZE_VARIANTS(name, w)

#define AMO_SIZE_VARIANT_BHW(name) \
        AMO_SIZE_VARIANT_HW(name) \
        AMO_SIZE_VARIANTS(name, b) \

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
#define VOP_ARITH(name) \
    template <typename T1, typename T2, typename T3, \
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0> \
    void name(const std::string &size, const T1 &dest, const T2 &src1, const T3 &src2) { \
        append(instruction(#name "." + size, def(dest), use(src1), use(src2))); \
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
    template <typename T1, typename T2, typename T3, typename T4,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0, is_reg<T4> = 0>
    void add(const T1 &dst,
             const T2 &pred,
             const T3 &src1,
             const T4 &src2) {
        append(instruction("add", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    template <typename T1, typename T2, typename T3, typename T4,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0, is_reg<T4> = 0>
    void sub(const T1 &dst,
             const T2 &pred,
             const T3 &src1,
             const T4 &src2) {
        append(instruction("sub", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void ld1(const T1 &src,
             const T2 &pred,
             const memory_operand &addr) {
        append(instruction("ld1", use(src), use(pred), use(addr)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void st1(const T1 &dest,
             const T2 &pred,
             const memory_operand &addr) {
        append(instruction("st1", def(dest), use(pred), use(addr)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void ptrue(const T1 &dest) {
        append(instruction("ptrue", def(dest)));
    }

    void insert_sep(const std::string &sep) { label(sep); }

    void insert_comment(std::string_view comment) {
        append(instruction(fmt::format("// {}", comment)));
    }

    bool has_label(const std::string &label) {
        auto label_str = label + ":";
        auto insn = instructions_;
        return std::any_of(insn.rbegin(), insn.rend(),
                            [&](const instruction &i) {
                                return i.opcode() == label_str;
                            });
    }

	void allocate();

	void emit(machine_code_writer &writer);

    std::size_t nr_instructions() const { return instructions_.size(); }

    const instruction_stream &instructions() const { return instructions_; }
private:
	instruction_stream instructions_;

	void append(const instruction &i) { instructions_.push_back(i); }

    void spill();
};
} // namespace arancini::output::dynamic::arm64

