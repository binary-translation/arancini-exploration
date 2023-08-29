/**
 * All of these implementations assume that the widths of the input registers are at least as big as the intended output.
 */
#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>


inline void not_(Assembler &assembler, TypedRegister &out, const TypedRegister &src)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		if (is_flag_t(out.type())) {
			assembler.xori(out, src, 1);
			out.set_type(value_type::u64());
		} else if (is_gpr_t(out.type())) {
			assembler.not_(out, src);
			out.set_actual_width();
			out.set_type(src.type());
		} else {
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}
