#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

namespace arancini::output::dynamic::arm64 {
class arm64_instruction_builder {
public:
	void add(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2) {
        append(arm64_instruction::add(dst, src1, src2));
    }

	void add(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2,
             const arm64_operand &shift) {
        append(arm64_instruction::add(dst, src1, src2, shift));
    }

	void sub(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2,
             const arm64_operand &shift) {
        append(arm64_instruction::sub(dst, src1, src2, shift));
    }

	void or_(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2) {
        append(arm64_instruction::or_(dst, src1, src2));
    }

	void and_(const arm64_operand &dst,
              const arm64_operand &src1,
              const arm64_operand &src2) {
        append(arm64_instruction::and_(dst, src1, src2));
    }

	void xor_(const arm64_operand &dst,
              const arm64_operand &src1,
              const arm64_operand &src2) {
        append(arm64_instruction::xor_(dst, src1, src2));
    }

	void not_(const arm64_operand &dst, const arm64_operand &src) {
        append(arm64_instruction::not_(dst, src));
    }

    void movn(const arm64_operand &dst,
              const arm64_operand &src,
              const arm64_operand &shift) {
        append(arm64_instruction::movn(dst, src, shift));
    }

    void movz(const arm64_operand &dst,
              const arm64_operand &src,
              const arm64_operand &shift) {
        append(arm64_instruction::movz(dst, src, shift));
    }

    void movk(const arm64_operand &dst,
              const arm64_operand &src,
              const arm64_operand &shift) {
        append(arm64_instruction::movk(dst, src, shift));
    }

    void mov(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::mov(dst, src)); }

	void setz(const arm64_operand &dst) {
        append(arm64_instruction::cset(dst, arm64_cond_operand("eq")));
    }

	void sets(const arm64_operand &dst) {
        append(arm64_instruction::cset(dst, arm64_cond_operand("lt")));
    }

	void setc(const arm64_operand &dst) {
        append(arm64_instruction::cset(dst, arm64_cond_operand("cs")));
    }

	void seto(const arm64_operand &dst) {
        append(arm64_instruction::cset(dst, arm64_cond_operand("vs")));
    }

    void b(const std::string &label) {
        append(arm64_instruction::b(label));
    }

    void beq(const std::string &label) {
        append(arm64_instruction::beq(label));
    }

    void label(const std::string &name) {
        append(arm64_instruction::label(name));
    }

    void lsl(const arm64_operand &dst,
             const arm64_operand &input,
             const arm64_operand &amount) {
        append(arm64_instruction::lsl(dst, input, amount));
    }

    void lsr(const arm64_operand &dst,
             const arm64_operand &input,
             const arm64_operand &amount) {
        append(arm64_instruction::lsr(dst, input, amount));
    }

    void asr(const arm64_operand &dst,
             const arm64_operand &input,
             const arm64_operand &amount) {
        append(arm64_instruction::asr(dst, input, amount));
    }

    void csel(const arm64_operand &dst,
              const arm64_operand &src1,
              const arm64_operand &src2,
              const arm64_operand &cond) {
        append(arm64_instruction::csel(dst, src1, src2, cond));
    }

    void csel(const arm64_operand &dst,
              const arm64_operand &cond) {
        append(arm64_instruction::cset(dst, cond));
    }

    void ubfx(const arm64_operand &dst,
              const arm64_operand &src1,
              const arm64_operand &src2,
              const arm64_operand &cond) {
        append(arm64_instruction::ubfx(dst, src1, src2, cond));
    }

    void bfi(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2,
             const arm64_operand &cond) {
        append(arm64_instruction::bfi(dst, src1, src2, cond));
    }

    void ldr(const arm64_operand &dst,
             const arm64_operand &base) {
        append(arm64_instruction::ldr(dst, base));
    }

    void str(const arm64_operand &dst,
             const arm64_operand &base) {
        append(arm64_instruction::str(dst, base));
    }

    void mul(const arm64_operand &dst,
             const arm64_operand &src1,
             const arm64_operand &src2) {
        append(arm64_instruction::mul(dst, src1, src2));
    }

    void sdiv(const arm64_operand &dst,
              const arm64_operand &src1,
              const arm64_operand &src2) {
        append(arm64_instruction::sdiv(dst, src1, src2));
    }

    void cmp(const arm64_operand &src1,
             const arm64_operand &src2) {
        append(arm64_instruction::cmp(src1, src2));
    }

    void bl(const std::string &n) {
        append(arm64_instruction::bl(n));
    }

    void ret() {
        append(arm64_instruction::ret());
    }

    // TODO: insert separators before/after instructions
    void insert_sep(const std::string &sep) { label(sep); }

    bool has_label(const std::string &label) {
        auto insn = instructions_;
        return std::any_of(insn.rbegin(), insn.rend(),
                            [&](const arm64_instruction &i) {
                                return i.opcode == label;
                            });
    }

	void allocate();

	void emit(machine_code_writer &writer);

	void dump(std::ostream &os) const;

	size_t nr_instructions() const { return instructions_.size(); }
private:
	std::vector<arm64_instruction> instructions_;

	void append(const arm64_instruction &i) { instructions_.push_back(i); }

    void spill();
};
} // namespace arancini::output::dynamic::arm64

