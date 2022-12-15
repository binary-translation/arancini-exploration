#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <stdexcept>

namespace arancini::output::dynamic::x86 {
enum class segment_override_prefix { none = 0, fs = 0x64, gs = 0x65 };

enum class operand_override_prefix { none = 0, operand_override = 0x66 };

enum class address_size_override_prefix { none = 0, address_size_override = 0x67 };

enum class raw_opcodes {
	nop = 0x90,
	mov_rm8_r8 = 0x88,
	mov_rm16_32_64_r16_32_64 = 0x89,
	mov_r8_rm8 = 0x8a,
	mov_r16_32_64_rm16_32_64 = 0x8b,
	mov_r8_imm8 = 0xb0,
	mov_r16_32_64_imm16_32_64 = 0xb8,
	mov_rm8_imm8 = 0xc6,
	mov_rm16_32_64_imm16_32 = 0xc7,
};

enum class encoding {
	zo, // no operands
	rm, // modrm:reg (w) / modrm:rm (r)
	mr, // modrm:reg (w) / modrm:rm (r)
	oi, // modrm:reg (w) / modrm:rm (r)
	mi, // modrm:reg (w) / modrm:rm (r)
};

class x86_seg_reg {
public:
	static x86_seg_reg none, fs, gs;
};

class x86_register {
public:
	static x86_register al, ah, ax, eax, rax;
	static x86_register cl, ch, cx, ecx, rcx;
	static x86_register dl, dh, dx, edx, rdx;
	static x86_register bl, bh, bx, ebx, rbx;
	static x86_register spl, sp, esp, rsp;
	static x86_register bpl, bp, ebp, rbp;
	static x86_register sil, si, esi, rsi;
	static x86_register dil, di, edi, rdi;
	static x86_register r8b, r8w, r8l, r8;
	static x86_register r9b, r9w, r9l, r9;
	static x86_register r10b, r10w, r10l, r10;
	static x86_register r11b, r11w, r11l, r11;
	static x86_register r12b, r12w, r12l, r12;
	static x86_register r13b, r13w, r13l, r13;
	static x86_register r14b, r14w, r14l, r14;
	static x86_register r15b, r15w, r15l, r15;
	static x86_register rip, riz;

	x86_register(int index, int width, bool hireg, bool newreg, bool virt = false)
		: index_(index)
		, width_(width)
		, hireg_(hireg)
		, newreg_(newreg)
		, virtual_(virt)
	{
	}

	static x86_register virt(int index, int width) { return x86_register(index, width, false, false, true); }

	int width() const { return width_; }
	int index() const { return index_; }

private:
	int index_, width_;
	bool hireg_, newreg_, virtual_;
};

class x86_memory {
public:
	x86_memory(int access_width, const x86_register &base, int displacement = 0)
		: access_width_(access_width)
		, base_(base)
		, seg_(x86_seg_reg::none)
		, index_(x86_register::riz)
		, scale_(1)
		, displacement_(displacement)
	{
	}

	x86_memory(int access_width, const x86_seg_reg &seg, const x86_register &base, int displacement = 0)
		: access_width_(access_width)
		, base_(base)
		, seg_(seg)
		, index_(x86_register::riz)
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
	x86_seg_reg seg_;
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

class x86_raw_instruction {
public:
	void emit(machine_code_writer &writer) const
	{
		// Prefixes

		if (segment_override != segment_override_prefix::none) {
			writer.emit8((unsigned char)segment_override);
		}

		if (operand_override != operand_override_prefix::none) {
			writer.emit8((unsigned char)operand_override);
		}

		if (address_override != address_size_override_prefix::none) {
			writer.emit8((unsigned char)address_override);
		}

		if (rex.bits != 0) {
			writer.emit8(rex.bits);
		}

		// Opcode
		if ((int)insn_opcode > 0x300) {
			writer.emit8(0x0f);
			writer.emit8(0x3a);
		} else if ((int)insn_opcode > 0x200) {
			writer.emit8(0x0f);
			writer.emit8(0x38);
		} else if ((int)insn_opcode > 0x100) {
			writer.emit8(0x0f);
		}

		writer.emit8((unsigned char)((int)insn_opcode & 0xff));

		switch (insn_encoding) {
		case encoding::rm:
		case encoding::mr:
			writer.emit8(modrm.bits);

			// TODO: SIB

			switch (modrm.parts.mod) {
			case 0: // indirect
				break;

			case 1: // indirect + disp8
				writer.emit8((unsigned char)displacement);
				break;

			case 2: // indirect + disp32
				writer.emit32(displacement);
				break;

			case 3: // direct
				break;

			default:
				throw std::runtime_error("illegal mod value");
			}
			break;

		case encoding::oi:
			switch (imm_width) {
			case 8:
				writer.emit8(immediate);
				break;
			case 16:
				writer.emit16(immediate);
				break;
			case 32:
				writer.emit32(immediate);
				break;
			case 64:
				writer.emit64(immediate);
				break;
			}
			break;

		case encoding::zo:
			break;

		default:
			throw std::runtime_error("unsupported operand encoding");
		}
	}

public:
	x86_raw_instruction()
		: segment_override(segment_override_prefix::none)
		, operand_override(operand_override_prefix::none)
		, address_override(address_size_override_prefix::none)
		, insn_opcode(raw_opcodes::nop)
		, insn_encoding(encoding::zo)
		, displacement(0)
		, immediate(0)
	{
		modrm.bits = 0;
		sib.bits = 0;
		rex.bits = 0;
	}

	segment_override_prefix segment_override;
	operand_override_prefix operand_override;
	address_size_override_prefix address_override;
	union {
		struct {
			unsigned char top : 4;
			unsigned char w : 1;
			unsigned char r : 1;
			unsigned char x : 1;
			unsigned char b : 1;
		} parts;
		unsigned char bits;
	} rex;
	raw_opcodes insn_opcode;
	encoding insn_encoding;
	union {
		struct {
			unsigned char rm : 3;
			unsigned char reg : 3;
			unsigned char mod : 2;
		} parts;

		unsigned char bits;
	} modrm;
	union {
		struct {
			unsigned char base : 3;
			unsigned char index : 3;
			unsigned char ss : 2;
		} parts;

		unsigned char bits;
	} sib;
	int displacement;
	unsigned long immediate;
	int imm_width;
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
