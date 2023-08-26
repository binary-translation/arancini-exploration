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
using is_reg = std::enable_if_t<is_one_of<T, preg_operand, vreg_operand>::value, int>;

template <typename T>
using is_imm = std::enable_if_t<std::is_same<T, immediate_operand>::value, int>;

template <typename T>
using is_reg_or_immediate = std::enable_if_t<std::disjunction<is_one_of<T, preg_operand, vreg_operand>,
                                                              std::is_same<T, immediate_operand>>::value,
                                                              int>;

class instruction_builder {
public:
    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void add(const T1 &dst,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("add", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void add(const T1 &dst,
             const T2 &src1,
             const T3 &src2,
             const shift_operand &shift) {
        append(instruction("add", def(dst), use(src1), use(src2), use(shift)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
	void adds(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("adds", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void adds(const T1 &dst,
              const T2 &src1,
              const T3 &src2,
              const shift_operand &shift) {
        append(instruction("adds", def(keep(dst)), use(src1), use(src2), use(shift)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void adcs(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("adcs", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void sub(const T1 &dst,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("sub", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void sub(const T1 &dst,
             const T2 &src1,
             const T3 &src2,
             const shift_operand &shift) {
        append(instruction("sub", def(dst), use(src1), use(src2), use(shift)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void subs(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("subs", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void subs(const T1 &dst,
              const T2 &src1,
              const T3 &src2,
              const shift_operand &shift) {
        append(instruction("subs", def(keep(dst)), use(src1), use(src2), use(shift)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void sbcs(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("sbcs", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void orr_(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("orr", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void and_(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("and", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void ands(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("ands", def(keep(dst)), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void eor_(const T1 &dst,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("eor", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void not_(const T1 &dst, const T2 &src) {
        append(instruction("mvn", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void neg(const T1 &dst, const T2 &src) {
        append(instruction("neg", def(dst), use(src)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movn(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        append(instruction("movn", def(dst), use(src), use(shift)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movz(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        append(instruction("movz", def(dst), use(src), use(shift)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void movk(const T1 &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        append(instruction("movk", usedef(dst), use(src), use(shift)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void mov(const T1 &dst, const T2 &src) {
        append(instruction("mov", def(dst), use(src)));
    }

    void b(const label_operand &dest) {
        append(instruction("b", use(dest)));
    }

    void beq(const label_operand &dest) {
        append(instruction("beq", use(dest)));
    }

    void bl(const label_operand &dest) {
        append(instruction("bl", use(dest)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void cmn(const T1 &dst,
             const T2 &src) {
        append(instruction("cmn", usedef(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg_or_immediate<T2> = 0>
    void cmp(const T1 &dst,
             const T2 &src) {
        append(instruction("cmp", usedef(dst), use(src)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void lsl(const T1 &dst,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("lsl", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void lsr(const T1 &dst,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("lsr", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg_or_immediate<T3> = 0>
    void asr(const T1 &dst,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("asr", def(dst), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void csel(const T1 &dst,
              const T2 &src1,
              const T3 &src2,
              const cond_operand &cond) {
        append(instruction("csel", def(dst), use(src1), use(src2), use(cond)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void cset(const T1 &dst,
              const cond_operand &cond) {
        append(instruction("cset", def(dst), use(cond)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void bfm(const T1 &dst,
             const T2 &src1,
             const immediate_operand &immr,
             const immediate_operand &imms) {
        append(instruction("bfm", def(dst), use(src1), use(immr), use(imms)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void bfi(const T1 &dst,
             const T2 &src1,
             const immediate_operand &immr,
             const immediate_operand &imms) {
        append(instruction("bfi", def(dst), use(src1), use(immr), use(imms)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void ldr(const T1 &dst,
             const memory_operand &base) {
        append(instruction("ldr", def(dst), use(base)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void str(const T1 &src,
             const memory_operand &base) {
        append(instruction("str", use(src), def(base)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void strb(const T1 &src,
             const memory_operand &base) {
        append(instruction("strb", use(src), def(base)));
    }

    template <typename T1,
              is_reg<T1> = 0>
    void strh(const T1 &src,
             const memory_operand &base) {
        append(instruction("strh", use(src), def(base)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void mul(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("mul", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void smulh(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("smulh", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void smull(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("smull", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void umulh(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("umulh", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void umull(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("umull", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void fmul(const T1 &dest,
             const T2 &src1,
             const T3 &src2) {
        append(instruction("fmul", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2, typename T3,
              is_reg<T1> = 0, is_reg<T2> = 0, is_reg<T3> = 0>
    void sdiv(const T1 &dest,
              const T2 &src1,
              const T3 &src2) {
        append(instruction("sdiv", def(dest), use(src1), use(src2)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtzs(const T1 &dest,
             const T2 &src) {
        append(instruction("fcvtzs", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtzu(const T1 &dest,
             const T2 &src) {
        append(instruction("fcvtzu", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtas(const T1 &dest,
             const T2 &src) {
        append(instruction("fcvtas", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void fcvtau(const T1 &dest,
             const T2 &src) {
        append(instruction("fcvtau", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void scvtf(const T1 &dest,
              const T2 &src) {
        append(instruction("scvtf", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void ucvtf(const T1 &dest,
              const T2 &src) {
        append(instruction("ucvtf", def(dest), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void msr(const T1 &dest,
             const T2 &src) {
        append(instruction("msr", usedef(dest), usedef(src)));
    }

    void ret() {
        append(instruction("ret"));
    }

    void brk(const immediate_operand &imm) {
        append(instruction("brk", use(imm)));
    }

    void label(const std::string &label) {
        append(instruction(label + ":"));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setz(const T1 &dst) {
        append(instruction("cset", def(dst), cond_operand("eq")));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void sets(const T1 &dst) {
        append(instruction("cset", def(dst), cond_operand("lt")));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setc(const T1 &dst) {
        append(instruction("cset", def(dst), cond_operand("cs")));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void setcc(const T1 &dst) {
        append(instruction("cset", def(dst), cond_operand("cc")));
    }

    template <typename T1,
              is_reg<T1> = 0>
	void seto(const T1 &dst) {
        append(instruction("cset", def(dst), cond_operand("vs")));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxtb(const T1 &dst, const T2 &src) {
        append(instruction("sxtb", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxth(const T1 &dst, const T2 &src) {
        append(instruction("sxth", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void sxtw(const T1 &dst, const T2 &src) {
        append(instruction("sxtw", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxtb(const T1 &dst, const T2 &src) {
        append(instruction("uxtb", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxth(const T1 &dst, const T2 &src) {
        append(instruction("uxth", def(dst), use(src)));
    }

    template <typename T1, typename T2,
              is_reg<T1> = 0, is_reg<T2> = 0>
    void uxtw(const T1 &dst, const T2 &src) {
        append(instruction("uxtw", def(dst), use(src)));
    }

    void insert_sep(const std::string &sep) { label(sep); }

    bool has_label(const std::string &label) {
        auto insn = instructions_;
        return std::any_of(insn.rbegin(), insn.rend(),
                            [&](const instruction &i) {
                                return i.opcode() == label;
                            });
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

