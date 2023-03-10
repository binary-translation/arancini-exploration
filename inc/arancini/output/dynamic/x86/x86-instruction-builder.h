#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/x86/x86-instruction.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace arancini::output::dynamic::x86 {
class x86_instruction_builder {
public:
	void mov(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::mov(dst, src)); }
	void movz(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::movz(dst, src)); }
	void movs(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::movs(dst, src)); }
	void xor_(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::xor_(dst, src)); }
	void and_(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::and_(dst, src)); }
	void or_(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::or_(dst, src)); }
	void add(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::add(dst, src)); }
	void sub(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::sub(dst, src)); }
	void mul(const x86_operand &dst, const x86_operand &src) { append(x86_instruction::mul(dst, src)); }
	void setz(const x86_operand &dst) { append(x86_instruction::setz(dst)); }
	void sets(const x86_operand &dst) { append(x86_instruction::sets(dst)); }
	void setc(const x86_operand &dst) { append(x86_instruction::setc(dst)); }
	void seto(const x86_operand &dst) { append(x86_instruction::seto(dst)); }
	void ret() { append(x86_instruction::ret()); }
	void int3() { append(x86_instruction::int3()); }

	void append(const x86_instruction &i) { instructions_.push_back(i); }

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
	std::vector<x86_instruction> instructions_;
};
} // namespace arancini::output::dynamic::x86
