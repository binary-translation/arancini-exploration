#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace arancini::output::dynamic::arm64 {
class arm64_instruction_builder {
public:
	void add(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::add(dst, src)); }

	void sub(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::sub(dst, src)); }

	void or_(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::or_(dst, src)); }

	void and_(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::and_(dst, src)); }

	void xor_(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::xor_(dst, src)); }

	void not_(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::not_(dst, src)); }

    void mov(const arm64_operand &dst, const arm64_operand &src) { append(arm64_instruction::mov(dst, src)); }

	void setz(const arm64_operand &dst) {
        append(arm64_instruction::moveq(dst, 1));
    }

	void sets(const arm64_operand &dst) {
        append(arm64_instruction::movss(dst, 1));
    }

	void setc(const arm64_operand &dst) {
        append(arm64_instruction::movcs(dst, 1));
    }

	void seto(const arm64_operand &dst) {
        append(arm64_instruction::movvs(dst, 1));
    }

	void append(const arm64_instruction &i) { instructions_.push_back(i); }

	void allocate();

	void emit(machine_code_writer &writer)
	{
		for (const auto &i : instructions_) {
			i.emit(writer);
		}
	}

	void dump(std::ostream &os) const;

	size_t nr_instructions() const { return instructions_.size(); }
private:
	std::vector<arm64_instruction> instructions_;
};
} // namespace arancini::output::dynamic::arm64

