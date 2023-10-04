#pragma once

#include <arancini/output/dynamic/riscv64/instruction-builder/builder.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

/**
 * Generate zero and sign flags based on the result in the given register
 * @param z whether zero flag should be generated
 * @param n whether sign flag should be generated
 */
void zero_sign_flag(InstructionBuilder &builder, TypedRegister &out, bool z, bool n)
{
	if (z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (z) {
				builder.seqz(ZF, out); // ZF
			}
			if (n) {
				builder.sltz(SF, out); // SF
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
inline void add_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, bool z, bool v, bool c, bool n)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (v) {
				extend_to_64(builder, lhs, lhs);
				extend_to_64(builder, rhs, rhs);

				builder.sltz(CF, lhs);
				builder.slt(OF, out, rhs);
				builder.xor_(OF, OF, CF); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (c) {
				extend_to_64(builder, rhs, rhs);

				builder.sltu(CF, out, rhs); // CF (Allows typical x86 case of lhs==out) FIXME Assumes out!=rhs
			}
			zero_sign_flag(builder, out, z, n);
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
	InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, intptr_t imm, bool z, bool v, bool c, bool n, Register of = OF, Register cf = CF)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (v) {
				extend_to_64(builder, lhs, lhs);

				builder.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm < 0) {
					builder.xori(of, of, 1); // Invert on negative
				}
			}
			if (c) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(cf, out, lhs); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, z, n);
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
inline void sub_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, bool z, bool v, bool c, bool n)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (v) {
				extend_to_64(builder, lhs, lhs);
				extend_to_64(builder, rhs, rhs);

				builder.sgtz(CF, rhs);
				builder.slt(OF, out, lhs);
				builder.xor_(OF, OF, CF); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (c) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(CF, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, z, n);
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
	InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, intptr_t imm, bool z, bool v, bool c, bool n, Register of = OF, Register cf = CF)
{
	if (v || c || z || n) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (v) {
				extend_to_64(builder, lhs, lhs);

				builder.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm > 0) {
					builder.xori(of, of, 1); // Invert on positive
				}
			}
			if (c) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(cf, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, z, n);
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
inline void mul_flags(InstructionBuilder &builder, TypedRegister &out, bool v, bool c)
{
	if (c || v) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			switch (out.type().element_width()) {
			case 128: {
				switch (out.type().element_type().type_class()) {
				case value_type_class::signed_integer:
					builder.srai(CF, out.reg1(), 63);
					builder.xor_(CF, CF, out.reg2());
					builder.snez(CF, CF);
					break;
				case value_type_class::unsigned_integer: {
					builder.snez(CF, out.reg2());
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
						builder.sextw(CF, out);
					} else {
						builder.slli(CF, out, 64 - (out.type().element_width()) / 2);
						builder.srai(CF, out, 64 - (out.type().element_width()) / 2);
					}
					builder.xor_(CF, CF, out);
					builder.snez(CF, CF);
					break;
				case value_type_class::unsigned_integer:
					builder.srli(CF, out, out.type().element_width() / 2);
					builder.snez(CF, CF);
					break;
				default:
					throw std::runtime_error("Unsupported value type for multiply");
				}

			} break;
			default:
				throw std::runtime_error("Unsupported width for sub immediate");
			}
			builder.mv(OF, CF);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}
