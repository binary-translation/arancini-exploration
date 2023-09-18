#pragma once

#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void truncate(Assembler &assembler, TypedRegister &out, const Register src)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 1:
			// Flags are always zero extended
			assembler.andi(out, src, 1);
			out.set_type(value_type::u64());
			break;
		case 8:
		case 16:
			// No op FIXME not sure if good idea
			if (src == out) {
				assembler.mv(out, src);
			}
			break;
		case 32:
			assembler.sextw(out, src);
			out.set_actual_width();
			out.set_type(value_type::u64());
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

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

inline void bit_insert(
	Assembler &assembler, TypedRegister &out, const TypedRegister& src, const TypedRegister& bits, const int to, const int length, const TypedRegister& temp_reg)
{
	int64_t mask = ~(((1ll << length) - 1) << to);

	if (!out.type().is_vector() && out.type().is_integer()) {
		const Register &src1 = out.type().element_width() == 128 ? src.reg1() : src;
		const Register &out1 = out.type().element_width() == 128 ? out.reg1() : out;
		//TODO Might be able to save masking of src or bits in some cases
		if (to == 0 && IsITypeImm(mask)) {
			// Since to==0 no shift necessary and just masking both is enough
			// `~mask` also fits IType since `mask` has all but lower bits set
			assembler.andi(temp_reg, bits, ~mask);
			assembler.andi(out1, src1, mask);
		} else {
			gen_constant(assembler, mask, temp_reg);
			assembler.and_(out1, src1, temp_reg);
			assembler.slli(temp_reg, bits, 64 - length);
			if (length + to != 64) {
				assembler.srli(temp_reg, temp_reg, 64 - (length + to));
			}

			// Might be able to set "bigger" out type here, but rather careful by keeping intended/explicit out type
		}

		assembler.or_(out1, out1, temp_reg);

	} else {
		throw std::runtime_error("not implemented");
	}
}
