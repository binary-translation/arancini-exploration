#pragma once

#include <iostream>
#include <list>

namespace arancini::output::dynamic {
class machine_code_writer;
}

namespace arancini::output::dynamic::x86 {
enum class opcodes { invalid, mov, o_and, o_or, o_xor, add, sub, setz, seto, setc, sets };

enum class operand_kind { none, reg, mem, immediate };

enum class operand_mode { read, write, readwrite };

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

class instruction_operand {
public:
	instruction_operand()
		: operand_(operand::none())
		, use_(false)
		, def_(false)
	{
	}

	instruction_operand(const operand &o, bool iuse = false, bool idef = false)
		: operand_(o)
		, use_(iuse)
		, def_(idef)
	{
	}

	static instruction_operand none() { return instruction_operand(operand::none()); }
	static instruction_operand use(const operand &o) { return instruction_operand(o, true, false); }
	static instruction_operand def(const operand &o) { return instruction_operand(o, false, true); }
	static instruction_operand usedef(const operand &o) { return instruction_operand(o, true, true); }

	const operand &oper() const { return operand_; }
	operand &oper() { return operand_; }

	bool is_use() const { return use_; }
	bool is_def() const { return def_; }
	bool is_usedef() const { return use_ && def_; }

	void dump(std::ostream &os) const;

private:
	operand operand_;
	bool use_;
	bool def_;
};

class instruction {
public:
	static const int NR_OPERANDS = 4;

	instruction(opcodes o)
		: opcode_(o)
	{
	}

	instruction(opcodes o, const instruction_operand &o1)
		: opcode_(o)
	{
		operands_[0] = o1;
	}

	instruction(opcodes o, const instruction_operand &o1, const instruction_operand &o2)
		: opcode_(o)
	{
		operands_[0] = o1;
		operands_[1] = o2;
	}

	void dump(std::ostream &os) const;
	void emit(machine_code_writer &writer) const;

	opcodes opcode() const { return opcode_; }

	const instruction_operand &get_operand(int o) const { return operands_[o]; }
	instruction_operand &get_operand(int o) { return operands_[o]; }

private:
	opcodes opcode_;
	instruction_operand operands_[NR_OPERANDS];
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

	void finalise(machine_code_writer &writer)
	{
		std::cerr << "BEFORE REGALLOC" << std::endl;
		dump(std::cerr);
		allocate();
		std::cerr << "AFTER REGALLOC" << std::endl;
		dump(std::cerr);
		std::cerr << "---" << std::endl;

		emit(writer);
	}

	void add_mov(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::mov, instruction_operand::use(src), instruction_operand::def(dst)));
	}

	void add_xor(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::o_xor, instruction_operand::use(src), instruction_operand::usedef(dst)));
	}

	void add_setz(const operand &dst) { add_instruction(instruction(opcodes::setz, instruction_operand::def(dst))); }
	void add_seto(const operand &dst) { add_instruction(instruction(opcodes::seto, instruction_operand::def(dst))); }
	void add_setc(const operand &dst) { add_instruction(instruction(opcodes::setc, instruction_operand::def(dst))); }
	void add_sets(const operand &dst) { add_instruction(instruction(opcodes::sets, instruction_operand::def(dst))); }

private:
	std::list<instruction> instructions_;

	void allocate();
	void emit(machine_code_writer &writer);
};
} // namespace arancini::output::dynamic::x86
