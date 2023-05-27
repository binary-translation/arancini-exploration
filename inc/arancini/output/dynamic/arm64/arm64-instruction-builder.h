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
        append(instruction::add(dst, src1, src2));
    }

	void add(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &shift) {
        append(instruction::add(dst, src1, src2, shift));
    }

	void sub(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction::sub(dst, src1, src2));
    }

	void sub(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &shift) {
        append(instruction::sub(dst, src1, src2, shift));
    }

	void or_(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction::or_(dst, src1, src2));
    }

	void and_(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction::and_(dst, src1, src2));
    }

	void xor_(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction::xor_(dst, src1, src2));
    }

	void not_(const operand &dst, const operand &src) {
        append(instruction::not_(dst, src));
    }

    void movn(const operand &dst,
              const operand &src,
              const operand &shift) {
        append(instruction::movn(dst, src, shift));
    }

    void movz(const operand &dst,
              const operand &src,
              const operand &shift) {
        append(instruction::movz(dst, src, shift));
    }

    void movk(const operand &dst,
              const operand &src,
              const operand &shift) {
        append(instruction::movk(dst, src, shift));
    }

    void mov(const operand &dst, const operand &src) { append(instruction::mov(dst, src)); }

	void setz(const operand &dst) {
        append(instruction::cset(dst, cond_operand("eq")));
    }

	void sets(const operand &dst) {
        append(instruction::cset(dst, cond_operand("lt")));
    }

	void setc(const operand &dst) {
        append(instruction::cset(dst, cond_operand("cs")));
    }

	void seto(const operand &dst) {
        append(instruction::cset(dst, cond_operand("vs")));
    }

    void b(const std::string &label) {
        append(instruction::b(label));
    }

    void beq(const std::string &label) {
        append(instruction::beq(label));
    }

    void label(const std::string &name) {
        append(instruction::label(name));
    }

    void lsl(const operand &dst,
             const operand &input,
             const operand &amount) {
        append(instruction::lsl(dst, input, amount));
    }

    void lsr(const operand &dst,
             const operand &input,
             const operand &amount) {
        append(instruction::lsr(dst, input, amount));
    }

    void asr(const operand &dst,
             const operand &input,
             const operand &amount) {
        append(instruction::asr(dst, input, amount));
    }

    void csel(const operand &dst,
              const operand &src1,
              const operand &src2,
              const operand &cond) {
        append(instruction::csel(dst, src1, src2, cond));
    }

    void csel(const operand &dst,
              const operand &cond) {
        append(instruction::cset(dst, cond));
    }

    void ubfx(const operand &dst,
              const operand &src1,
              const operand &src2,
              const operand &cond) {
        append(instruction::ubfx(dst, src1, src2, cond));
    }

    void bfi(const operand &dst,
             const operand &src1,
             const operand &src2,
             const operand &cond) {
        append(instruction::bfi(dst, src1, src2, cond));
    }

    void ldr(const operand &dst,
             const operand &base) {
        append(instruction::ldr(dst, base));
    }

    void str(const operand &dst,
             const operand &base) {
        append(instruction::str(dst, base));
    }

    void mul(const operand &dst,
             const operand &src1,
             const operand &src2) {
        append(instruction::mul(dst, src1, src2));
    }

    void sdiv(const operand &dst,
              const operand &src1,
              const operand &src2) {
        append(instruction::sdiv(dst, src1, src2));
    }

    void cmp(const operand &src1,
             const operand &src2) {
        append(instruction::cmp(src1, src2));
    }

    void bl(const std::string &n) {
        append(instruction::bl(n));
    }

    void ret() {
        append(instruction::ret());
    }

    void brk(const operand &imm) {
        append(instruction::brk(imm));
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

