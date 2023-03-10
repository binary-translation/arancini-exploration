#pragma once

#include <iostream>
#include <list>

namespace arancini::output::dynamic {
class machine_code_writer;
}

namespace arancini::output::dynamic::x86 {

// #define DEFOP(n) n,

enum class opcodes { invalid, mov, movz, movs, o_and, o_or, o_xor, add, sub, setz, seto, setc, sets };

// #undef DEFOP

enum class operand_kind { none, reg, mem, immediate };

enum class operand_mode { read, write, readwrite };

enum class physreg { rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15 };
enum class segreg { none, fs, gs };

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

	void allocate(physreg p)
	{
		kind = regref_kind::phys;
		preg_i = p;
	}
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

	static operand mem(int width, const regref &base, int displ) { return mem(width, segreg::none, base, displ); };

	static operand mem(int width, segreg seg, const regref &base, int displ)
	{
		operand r;
		r.kind = operand_kind::mem;
		r.width = width;
		r.mem_i.seg = seg;
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
			segreg seg;
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

enum class iform {
	invalid,
	f_n,
	f_r,
	f_m,
	f_rr,
	f_rm,
	f_mr,
	f_ir,
	f_im,
};

// enum raw_opcodes { mov_rr = 0x89, mov_rm = 0x89, mov_mr = 0x8b, mov_ir = 0xb8, mov_im = 0xc7 };

class instruction {
public:
	static const int NR_OPERANDS = 4;

	instruction(opcodes o)
		: opcode_(o)
		, iform_(iform::invalid)
	{
	}

	instruction(opcodes o, const instruction_operand &o1)
		: opcode_(o)
		, iform_(iform::invalid)
	{
		operands_[0] = o1;
	}

	instruction(opcodes o, const instruction_operand &o1, const instruction_operand &o2)
		: opcode_(o)
		, iform_(iform::invalid)
	{
		operands_[0] = o1;
		operands_[1] = o2;
	}

	void dump(std::ostream &os) const;
	void emit(machine_code_writer &writer) const;
	void kill() { opcode_ = opcodes::invalid; }

	bool dead() const { return opcode_ == opcodes::invalid; }

	opcodes opcode() const { return opcode_; }

	const instruction_operand &get_operand(int o) const { return operands_[o]; }
	instruction_operand &get_operand(int o) { return operands_[o]; }

	iform get_iform() const
	{
		if (iform_ != iform::invalid) {
			return iform_;
		}

		switch (operands_[0].oper().kind) {
		case operand_kind::none:
			iform_ = iform::f_n;
			break;

		case operand_kind::reg:
			switch (operands_[1].oper().kind) {
			case operand_kind::none:
				iform_ = iform::f_r;
				break;
			case operand_kind::reg:
				iform_ = iform::f_rr;
				break;
			case operand_kind::mem:
				iform_ = iform::f_rm;
				break;
			}

		case operand_kind::mem:
			switch (operands_[1].oper().kind) {
			case operand_kind::none:
				iform_ = iform::f_m;
				break;
			case operand_kind::reg:
				iform_ = iform::f_mr;
				break;
			}

		case operand_kind::immediate:
			switch (operands_[1].oper().kind) {
			case operand_kind::reg:
				iform_ = iform::f_ir;
				break;
			case operand_kind::mem:
				iform_ = iform::f_im;
				break;
			}
		}

		return iform_;
	}

private:
	opcodes opcode_;
	mutable iform iform_;
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
		allocate();
		emit(writer);
	}

	void add_mov(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::mov, instruction_operand::use(src), instruction_operand::def(dst)));
	}

	void add_movz(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::movz, instruction_operand::use(src), instruction_operand::def(dst)));
	}

	void add_movs(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::movs, instruction_operand::use(src), instruction_operand::def(dst)));
	}

	void add_xor(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::o_xor, instruction_operand::use(src), instruction_operand::usedef(dst)));
	}

	void add_and(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::o_and, instruction_operand::use(src), instruction_operand::usedef(dst)));
	}

	void add_add(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::add, instruction_operand::use(src), instruction_operand::usedef(dst)));
	}

	void add_sub(const operand &src, const operand &dst)
	{
		add_instruction(instruction(opcodes::sub, instruction_operand::use(src), instruction_operand::usedef(dst)));
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
