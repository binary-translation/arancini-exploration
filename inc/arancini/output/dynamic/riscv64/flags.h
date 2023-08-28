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
