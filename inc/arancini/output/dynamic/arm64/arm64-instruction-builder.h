#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

namespace arancini::output::dynamic::arm64 {
class instruction_builder {
public:
	void add(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("add", def(dst), use(src1), use(src2)));
    }

    void add(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &shift) {
        append(instruction("add", def(dst), use(src1), use(src2), use(shift)));
    }

	void adds(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction("adds", def(dst), use(src1), use(src2)));
    }

    void adds(const operand &dst,
              const operand &src1,
              const operand &src2,
              const operand &shift) {
        append(instruction("adds", usedef(dst), use(src1), use(src2), use(shift)));
    }

    void sub(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("sub", def(dst), use(src1), use(src2)));
    }

    void sub(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &shift) {
        append(instruction("sub", def(dst), use(src1), use(src2), use(shift)));
    }

    void subs(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("subs", usedef(dst), use(src1), use(src2)));
    }

    void subs(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &shift) {
        append(instruction("subs", usedef(dst), use(src1), use(src2), use(shift)));
    }

    void orr_(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction("orr", def(dst), use(src1), use(src2)));
    }

    void and_(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction("and", usedef(dst), use(src1), use(src2)));
    }

    void ands(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction("ands", usedef(dst), use(src1), use(src2)));
    }

    void eor_(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction("eor", def(dst), use(src1), use(src2)));
    }

    void not_(const operand &dst, const operand &src) {
        append(instruction("mvn", def(dst), use(src)));
    }

    void moveq(const operand &dst, const immediate_operand &src) {
        append(instruction("moveq", def(dst), use(src)));
    }

    void movss(const operand &dst, const immediate_operand &src) {
        append(instruction("movss", def(dst), use(src)));
    }

    void movvs(const operand &dst, const immediate_operand &src) {
        append(instruction("movvs", def(dst), use(src)));
    }

    void movcs(const operand &dst, const immediate_operand &src) {
        append(instruction("movcs", def(dst), use(src)));
    }

    void movn(const operand &dst,
                            const operand &src,
                            const operand &shift) {
        append(instruction("movn", def(dst), use(src), use(shift)));
    }

    void movz(const operand &dst,
                            const operand &src,
                            const operand &shift) {
        append(instruction("movz", def(dst), use(src), use(shift)));
    }

    void movk(const operand &dst,
                            const operand &src,
                            const operand &shift) {
        append(instruction("movk", def(dst), use(src), use(shift)));
    }

    void mov(const operand &dst, const operand &src) {
        append(instruction("mov", def(dst), use(src)));
    }

    void b(const std::string &src) {
        append(instruction("b", use(label_operand(src))));
    }

    void beq(const std::string &src) {
        append(instruction("beq", use(label_operand(src))));
    }

    void bl(const std::string &name) {
        append(instruction("bl", use(label_operand(name))));
    }

    void cmp(const operand &src1,
             const operand &src2) {
        append(instruction("cmp", use(src1), use(src2)));
    }

    void cmp(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("cmp", def(dst), use(src1), use(src2)));
    }

    void lsl(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("lsl", def(dst), use(src1), use(src2)));
    }

    void lsr(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("lsr", def(dst), use(src1), use(src2)));
    }

    void asr(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction("asr", def(dst), use(src1), use(src2)));
    }

    void csel(const operand &dst,
                            const operand &src1,
                            const operand &src2,
                            const operand &cond) {
        append(instruction("csel", def(dst), use(src1), use(src2), use(cond)));
    }

    void cset(const operand &dst,
              const operand &cond) {
        append(instruction("cset", def(dst), use(cond)));
    }

    void ubfx(const operand &dst,
              const operand &src1,
              const operand &src2,
              const operand &cond) {
        append(instruction("ubfx", def(dst), use(src1), use(src2), use(cond)));
    }

    void bfi(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &cond) {
        append(instruction("bfi", def(dst), use(src1), use(src2), use(cond)));
    }

    void ldr(const operand &dst,
             const operand &base) {
        append(instruction("ldr", def(dst), use(base)));
    }

    void str(const operand &src,
             const operand &base) {
        append(instruction("str", use(src), def(base)));
    }

    void mul(const operand &dest,
             const operand &src1,
             const operand &src2) {
        append(instruction("mul", def(dest), use(src1), use(src2)));
    }

    void sdiv(const operand &dest,
              const operand &src1,
              const operand &src2) {
        append(instruction("sdiv", def(dest), use(src1), use(src2)));
    }

    void ret() {
        append(instruction("ret"));
    }

    void brk(const operand &imm) {
        append(instruction("brk", use(imm)));
    }

    void label(const std::string &label) {
        append(instruction(label + ":"));
    }

	void setz(const operand &dst) {
        append(instruction("cset", def(dst), cond_operand("eq")));
    }

	void sets(const operand &dst) {
        append(instruction("cset", def(dst), cond_operand("lt")));
    }

	void setc(const operand &dst) {
        append(instruction("cset", def(dst), cond_operand("cs")));
    }

	void seto(const operand &dst) {
        append(instruction("cset", def(dst), cond_operand("vs")));
    }

    void sxtw(const operand &dst, const operand &src) {
        append(instruction("sxtw", def(dst), use(src)));
    }

    // TODO: insert separators before/after instructions
    void insert_sep(const std::string &sep) { label(sep); }

    bool has_label(const std::string &label) {
        auto insn = instructions_;
        return std::any_of(insn.rbegin(), insn.rend(),
                            [&](const instruction &i) {
                                return i.opcode == label;
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

