#pragma once

#include <list>
#include <ostream>

namespace arancini::output::dynamic::x86 {
enum class opcodes { mov, o_and, o_or, o_xor, add, sub, setz, seto, setc, sets };

enum class operand_kind { none, reg, mem, immediate };

enum class physreg { rax, rcx, rdx, rbx, rsi, rdi, rsp, rbp };

enum class regref_kind { phys, virt };

struct regref {
	static regref preg(physreg reg)
	{
		regref r;
		r.kind = regref_kind::phys;
		r.preg_i = reg;
		return r;
	}

	static regref vreg(int reg)
	{
		regref r;
		r.kind = regref_kind::virt;
		r.vreg_i = reg;
		return r;
	}

	regref_kind kind;
	union {
		physreg preg_i;
		int vreg_i;
	};

	void dump(std::ostream &os) const;
};

class operand {
public:
	static operand none()
	{
		operand r;
		r.kind = operand_kind::none;
		r.width = 0;
		return r;
	}

	static operand preg(int width, physreg reg)
	{
		operand r;
		r.kind = operand_kind::reg;
		r.width = width;
		r.reg_i.rr = regref::preg(reg);
		return r;
	}

	static operand vreg(int width, int reg)
	{
		operand r;
		r.kind = operand_kind::reg;
		r.width = width;
		r.reg_i.rr = regref::vreg(reg);
		return r;
	}

	static operand imm(int width, unsigned long val)
	{
		operand r;
		r.kind = operand_kind::immediate;
		r.width = width;
		r.imm_i.val = val;

		return r;
	}

	static operand mem(int width, const regref &base) { return mem(width, base, 0); }

	static operand mem(int width, const regref &base, int displ)
	{
		operand r;
		r.kind = operand_kind::mem;
		r.width = width;
		r.mem_i.base = base;
		r.mem_i.displacement = displ;

		return r;
	};

	operand_kind kind;
	int width;

	union {
		struct {
			regref rr;
		} reg_i;

		struct {
			regref base;
			int displacement;
		} mem_i;

		struct {
			unsigned long val;
		} imm_i;
	};

	void dump(std::ostream &os) const;
};

class instruction {
public:
	instruction(opcodes o)
		: opcode(o)
	{
		operands[0] = operand::none();
		operands[1] = operand::none();
		operands[2] = operand::none();
		operands[3] = operand::none();
	}

	instruction(opcodes o, const operand &o1)
		: opcode(o)
	{
		operands[0] = o1;
		operands[1] = operand::none();
		operands[2] = operand::none();
		operands[3] = operand::none();
	}

	instruction(opcodes o, const operand &o1, const operand &o2)
		: opcode(o)
	{
		operands[0] = o1;
		operands[1] = o2;
		operands[2] = operand::none();
		operands[3] = operand::none();
	}

	opcodes opcode;
	operand operands[4];

	void dump(std::ostream &os) const;
};

class machine_code_builder {
public:
	void add_instruction(const instruction &mci) { instructions_.push_back(mci); }

	void dump(std::ostream &os) const
	{
		for (const auto &i : instructions_) {
			i.dump(os);
		}
	}

	///

	void add_mov(const operand &src, const operand &dst) { add_instruction(instruction(opcodes::mov, src, dst)); }
	void add_xor(const operand &src, const operand &dst) { add_instruction(instruction(opcodes::o_xor, src, dst)); }
	void add_setz(const operand &dst) { add_instruction(instruction(opcodes::setz, dst)); }
	void add_seto(const operand &dst) { add_instruction(instruction(opcodes::seto, dst)); }
	void add_setc(const operand &dst) { add_instruction(instruction(opcodes::setc, dst)); }
	void add_sets(const operand &dst) { add_instruction(instruction(opcodes::sets, dst)); }

private:
	std::list<instruction> instructions_;
};
} // namespace arancini::output::dynamic::x86
