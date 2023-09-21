/**
 * All of these implementations assume that the widths of the input reg is at least as big as the intended output.
 */
#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void slli(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, intptr_t amt)
{
	switch (out_reg.type().element_width()) {
	case 128:
		assembler.slli(out_reg.reg2(), src_reg.reg2(), amt);
		assembler.srli(out_reg.reg1(), src_reg.reg1(), 64 - amt);
		assembler.or_(out_reg.reg2(), out_reg.reg2(), out_reg.reg1());
		assembler.slli(out_reg.reg1(), src_reg.reg1(), amt);
		break;
	case 64:
		assembler.slli(out_reg, src_reg, amt);
		break;
	case 32:
		assembler.slliw(out_reg, src_reg, amt);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 8:
	case 16:
		// TODO (Could possibly save 1 instruction by doing extension here (with other shift amounts) instead of later)
		assembler.slli(out_reg, src_reg, amt);
		break;
	}
}

inline void srli(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, intptr_t amt)
{
	switch (out_reg.type().element_width()) {
	case 128:
		assembler.srli(out_reg.reg1(), src_reg.reg1(), amt);
		assembler.slli(out_reg.reg2(), src_reg.reg2(), 64 - amt);
		assembler.or_(out_reg.reg1(), out_reg.reg1(), out_reg.reg2());
		assembler.srli(out_reg.reg2(), src_reg.reg2(), amt);
		break;
	case 64:
		assembler.srli(out_reg, src_reg, amt);
		break;
	case 32:
		assembler.srliw(out_reg, src_reg, amt);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 16:
	case 8:
		if (!fixup(assembler, out_reg, src_reg, value_type::u32(), -amt))
			assembler.srliw(out_reg, out_reg, amt);
		break;
	}
}

inline void srai(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, intptr_t amt)
{
	switch (out_reg.type().element_width()) {
	case 64:
		assembler.srai(out_reg, src_reg, amt);
		break;
	case 32:
		assembler.sraiw(out_reg, src_reg, amt);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 16:
	case 8:
		if (!fixup(assembler, out_reg, src_reg, value_type::s32(), -amt))
			assembler.sraiw(out_reg, out_reg, amt);
		break;
	}
}

inline void sll(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, const TypedRegister& amt_reg)
{
	switch (out_reg.type().element_width()) {
	case 64:
		assembler.sll(out_reg, src_reg, amt_reg);
		break;
	case 32:
		assembler.sllw(out_reg, src_reg, amt_reg);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 8:
	case 16:
		assembler.sll(out_reg, src_reg, amt_reg);
		break;
	}
}

inline void srl(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, const TypedRegister& amt_reg)
{
	switch (out_reg.type().element_width()) {
	case 64:
		assembler.srl(out_reg, src_reg, amt_reg);
		break;
	case 32:
		assembler.srlw(out_reg, src_reg, amt_reg);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 16:
	case 8:
		fixup(assembler, out_reg, src_reg, value_type::u32());
		assembler.srlw(out_reg, out_reg, amt_reg);
		break;
	}
}

inline void sra(Assembler &assembler, TypedRegister &out_reg, const TypedRegister& src_reg, const TypedRegister& amt_reg)
{
	switch (out_reg.type().element_width()) {
	case 64:
		assembler.sra(out_reg, src_reg, amt_reg);
		break;
	case 32:
		assembler.sraw(out_reg, src_reg, amt_reg);
		out_reg.set_actual_width();
		out_reg.set_type(value_type::u64());
		break;
	case 16:
	case 8:
		fixup(assembler, out_reg, src_reg, value_type::s32());
		assembler.sraw(out_reg, out_reg, amt_reg);
		break;
	}
}
