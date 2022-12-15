#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/x86/x86-instruction.h>
#include <stdexcept>
#include <vector>

namespace arancini::output::dynamic::x86 {
class x86_instruction_builder {
public:
	template <class D, class S> void mov(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::mov, x86_operand::write(dst), x86_operand::read(src)));
	}
	template <class D, class S> void movz(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::movz, x86_operand::write(dst), x86_operand::read(src)));
	}
	template <class D, class S> void movs(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::movs, x86_operand::write(dst), x86_operand::read(src)));
	}
	template <class D, class S> void xor_(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::xor_, x86_operand::readwrite(dst), x86_operand::read(src)));
	}
	template <class D, class S> void and_(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::and_, x86_operand::readwrite(dst), x86_operand::read(src)));
	}
	template <class D, class S> void add(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::add, x86_operand::readwrite(dst), x86_operand::read(src)));
	}
	template <class D, class S> void sub(const D &dst, const S &src)
	{
		instructions_.push_back(x86_instruction(opcodes::sub, x86_operand::readwrite(dst), x86_operand::read(src)));
	}
	template <class D> void setz(const D &dst) { instructions_.push_back(x86_instruction(opcodes::setz, x86_operand::write(dst))); }
	template <class D> void seto(const D &dst) { instructions_.push_back(x86_instruction(opcodes::seto, x86_operand::write(dst))); }
	template <class D> void setc(const D &dst) { instructions_.push_back(x86_instruction(opcodes::setc, x86_operand::write(dst))); }
	template <class D> void sets(const D &dst) { instructions_.push_back(x86_instruction(opcodes::sets, x86_operand::write(dst))); }

	void allocate();

	void emit(machine_code_writer &writer)
	{
		for (const auto &i : instructions_) {
			i.emit(writer);
		}
	}

private:
	std::vector<x86_instruction> instructions_;
};
} // namespace arancini::output::dynamic::x86
