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
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input registers assuming the operation was an addition.
 * @param z whether zero flag should be generated
 * @param v whether overflow flag should be generated
 * @param c whether carry flag should be generated
 * @param n whether sign flag should be generated
 */
inline void add_flags(Assembler &assembler, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, bool z, bool v, bool c, bool n)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(assembler, out, out);
			if (v) {
				extend_to_64(assembler, lhs, lhs);
				extend_to_64(assembler, rhs, rhs);

				assembler.sltz(CF, lhs);
				assembler.slt(OF, out, rhs);
				assembler.xor_(OF, OF, CF); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (c) {
				extend_to_64(assembler, rhs, rhs);

				assembler.sltu(CF, out, rhs); // CF (Allows typical x86 case of lhs==out) FIXME Assumes out!=rhs
			}
			zero_sign_flag(assembler, out, z, n);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input register and immediate assuming the operation was
 * an addition.
 * @param z whether zero flag should be generated
 * @param v whether overflow flag should be generated
 * @param c whether carry flag should be generated
 * @param n whether sign flag should be generated
 * @param of Optional. Register override for overflow flag
 * @param cf Optional. Register override for carry flag
 */
inline void addi_flags(
	Assembler &assembler, TypedRegister &out, TypedRegister &lhs, intptr_t imm, bool z, bool v, bool c, bool n, Register of = OF, Register cf = CF)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(assembler, out, out);
			if (v) {
				extend_to_64(assembler, lhs, lhs);

				assembler.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm < 0) {
					assembler.xori(of, of, 1); // Invert on negative
				}
			}
			if (c) {
				extend_to_64(assembler, lhs, lhs);

				assembler.sltu(cf, out, lhs); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(assembler, out, z, n);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input registers assuming the operation was a
 * subtraction.
 * @param z whether zero flag should be generated
 * @param v whether overflow flag should be generated
 * @param c whether carry flag should be generated
 * @param n whether sign flag should be generated
 */
inline void sub_flags(Assembler &assembler, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, bool z, bool v, bool c, bool n)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(assembler, out, out);
			if (v) {
				extend_to_64(assembler, lhs, lhs);
				extend_to_64(assembler, rhs, rhs);

				assembler.sgtz(CF, rhs);
				assembler.slt(OF, out, lhs);
				assembler.xor_(OF, OF, CF); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (c) {
				extend_to_64(assembler, lhs, lhs);

				assembler.sltu(CF, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(assembler, out, z, n);
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

/**
 * Generate overflow and carry flags based on the result in the given register and the given input register and immediate assuming the operation was a
 * multiplication.
 * @param v whether overflow flag should be generated
 * @param c whether carry flag should be generated
 */
inline void mul_flags(Assembler &assembler, TypedRegister &out, bool v, bool c)
{
	if (c || v) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			switch (out.type().element_width()) {
			case 128: {
				switch (out.type().element_type().type_class()) {
				case value_type_class::signed_integer:
					assembler.srai(CF, out.reg1(), 63);
					assembler.xor_(CF, CF, out.reg2());
					assembler.snez(CF, CF);
					break;
				case value_type_class::unsigned_integer: {
					assembler.snez(CF, out.reg2());
				} break;
				default:
					throw std::runtime_error("Unsupported value type for multiply");
				}
			} break;
			case 64:
			case 32:
			case 16: {
				switch (out.type().element_type().type_class()) {
				case value_type_class::signed_integer:
					if (out.type().element_width() == 64) {
						assembler.sextw(CF, out);
					} else {
						assembler.slli(CF, out, 64 - (out.type().element_width()) / 2);
						assembler.srai(CF, out, 64 - (out.type().element_width()) / 2);
					}
					assembler.xor_(CF, CF, out);
					assembler.snez(CF, CF);
					break;
				case value_type_class::unsigned_integer:
					assembler.srli(CF, out, out.type().element_width() / 2);
					assembler.snez(CF, CF);
					break;
				default:
					throw std::runtime_error("Unsupported value type for multiply");
				}

			} break;
			default:
				throw std::runtime_error("Unsupported width for sub immediate");
			}
			assembler.mv(OF, CF);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}
