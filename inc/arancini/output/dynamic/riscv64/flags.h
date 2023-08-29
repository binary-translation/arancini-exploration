#pragma once

#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

/**
 * Generate zero and sign flags based on the result in the given register
 * @param z whether zero flag should be generated
 * @param n whether sign flag should be generated
 */
void zero_sign_flag(Assembler &assembler, TypedRegister &out, bool z, bool n)
{
	if (z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(assembler, out, out);
			if (z) {
				assembler.seqz(ZF, out); // ZF
			}
			if (n) {
				assembler.sltz(SF, out); // SF
			}
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input register and immediate assuming the operation was
 * a subtraction.
 * @param z whether zero flag should be generated
 * @param v whether overflow flag should be generated
 * @param c whether carry flag should be generated
 * @param n whether sign flag should be generated
 * @param of Optional. Register override for overflow flag
 * @param cf Optional. Register override for carry flag
 */
inline void subi_flags(
	Assembler &assembler, TypedRegister &out, TypedRegister &lhs, intptr_t imm, bool z, bool v, bool c, bool n, Register of = OF, Register cf = CF)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(assembler, out, out);
			if (v) {
				extend_to_64(assembler, lhs, lhs);

				assembler.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm > 0) {
					assembler.xori(of, of, 1); // Invert on positive
				}
			}
			if (c) {
				extend_to_64(assembler, lhs, lhs);

				assembler.sltu(cf, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(assembler, out, z, n);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}
