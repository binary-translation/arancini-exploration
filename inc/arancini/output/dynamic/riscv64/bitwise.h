#pragma once

#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void bit_extract(Assembler &assembler, TypedRegister &out, const TypedRegister& src, int from, int length)
{
	if (is_flag_t(out.type()) && is_gpr_t(src.type())) {
		if (from == 0) {
			assembler.andi(out, src, 1);
		} else if (from == 63) {
			assembler.srli(out, src, 63);
		} else if (from == 31) {
			assembler.srliw(out, src, 31);
		} else {
			assembler.srli(out, src, from);
			assembler.andi(out, out, 1);
		}
		out.set_type(value_type::u64());
		return;
	}

	if (!(is_gpr_t(out.type()) && is_gpr_t(src.type()))) {
		throw std::runtime_error("Unsupported bit extract width.");
	}

	if (from == 0 && length == 32) {
		assembler.sextw(out, src);
		out.set_actual_width(32);
		out.set_type(value_type::u64());
		return;
	}

	Register temp = length + from < 64 ? out : src;
	if (length + from < 64) {
		assembler.slli(out, src, 64 - (from + length));
	}
	assembler.srai(out, temp, 64 - length); // Use arithmetic shift to keep sign extension up
	out.set_actual_width();
	out.set_type(value_type::u64());
}
