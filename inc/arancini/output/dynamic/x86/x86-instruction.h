#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <stdexcept>

namespace arancini::output::dynamic::x86 {

struct x86_register {
	enum x86_regname {
		al, ah, ax, eax, rax, //
		bl, bh, bx, ebx, rbx, //
		cl, ch, cx, ecx, rcx, //
		dl, dh, dx, edx, rdx, //
		dil, di, edi, rdi,//
		sil, si, esi, rsi,//
		spl, sp, esp, rsp,//
		bpl, bp, ebp, rbp,//
		rip,//
		fs, gs,//
		virt, none } regname;

	union {
		struct {
			int virt_index;
			int virt_width;
		};
	};

	x86_register()
		: regname(none)
	{
	}
	x86_register(x86_regname r)
		: regname(r)
	{
	}
	x86_register(x86_regname r, int vi, int vw)
		: regname(r)
		, virt_index(vi)
		, virt_width(vw)
	{
	}

	static x86_register preg(x86_regname r) { return x86_register(r); }
	static x86_register vreg(int i, int w) { return x86_register(virt, i, w); }
};

class x86_memory {
public:
	x86_memory(int access_width, const x86_register &base, int displacement = 0)
		: access_width_(access_width)
		, base_(base)
		, seg_(x86_register { x86_register::none })
		, index_(x86_register { x86_register::none })
		, scale_(1)
		, displacement_(displacement)
	{
	}

	x86_memory(int access_width, const x86_register &seg, const x86_register &base, int displacement = 0)
		: access_width_(access_width)
		, base_(base)
		, seg_(seg)
		, index_(x86_register { x86_register::none })
		, scale_(1)
		, displacement_(displacement)
	{
	}

	int access_width() const { return access_width_; }

	const x86_register &base() const { return base_; }

	int displacement() const { return displacement_; }

private:
	int access_width_;
	x86_register base_;
	x86_register seg_;
	x86_register index_;
	int scale_;
	int displacement_;
};

class x86_immediate {
public:
	x86_immediate(int width, unsigned long value)
		: width_(width)
		, value_(value)
	{
	}

	int width() const { return width_; }
	unsigned long value() const { return value_; }

private:
	int width_;
	unsigned long value_;
};

enum class opcodes { mov, movz, movs, xor_, and_, add, sub, setz, seto, setc, sets };

enum operand_type { invalid, regop, memory, immediate };

class x86_operand {
public:
	x86_operand()
		: type(operand_type::invalid)
		, read_(false)
		, write_(false)
	{
	}

	operand_type type;
	bool read_, write_;
	union {
		x86_register reg;
		x86_memory mem;
		x86_immediate imm;
	};

	static x86_operand read(const x86_register &reg)
	{
		x86_operand o;
		o.type = operand_type::regop;
		o.read_ = true;
		o.write_ = false;
		o.reg = reg;

		return o;
	}

	static x86_operand readwrite(const x86_register &reg)
	{
		x86_operand o;
		o.type = operand_type::regop;
		o.read_ = true;
		o.write_ = true;
		o.reg = reg;

		return o;
	}

	static x86_operand write(const x86_register &reg)
	{
		x86_operand o;
		o.type = operand_type::regop;
		o.read_ = false;
		o.write_ = true;
		o.reg = reg;

		return o;
	}

	static x86_operand read(const x86_memory &mem)
	{
		x86_operand o;
		o.type = operand_type::memory;
		o.read_ = true;
		o.write_ = false;
		o.mem = mem;

		return o;
	}

	static x86_operand readwrite(const x86_memory &mem)
	{
		x86_operand o;
		o.type = operand_type::memory;
		o.read_ = true;
		o.write_ = true;
		o.mem = mem;

		return o;
	}

	static x86_operand write(const x86_memory &mem)
	{
		x86_operand o;
		o.type = operand_type::memory;
		o.read_ = false;
		o.write_ = true;
		o.mem = mem;

		return o;
	}

	static x86_operand read(const x86_immediate &imm)
	{
		x86_operand o;
		o.type = operand_type::immediate;
		o.read_ = true;
		o.write_ = false;
		o.imm = imm;

		return o;
	}
};

class x86_instruction {
public:
	x86_instruction(opcodes opcode)
		: opcode_(opcode)
	{
	}

	x86_instruction(opcodes opcode, const x86_operand &o1)
		: opcode_(opcode)
	{
		operands[0] = o1;
	}

	x86_instruction(opcodes opcode, const x86_operand &o1, const x86_operand &o2)
		: opcode_(opcode)
	{
		operands[0] = o1;
		operands[1] = o2;
	}

	void emit(machine_code_writer &writer) const;

private:
	opcodes opcode_;
	x86_operand operands[4];
};

} // namespace arancini::output::dynamic::x86
