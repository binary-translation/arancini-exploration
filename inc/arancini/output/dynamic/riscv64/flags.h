#pragma once

#include <arancini/output/dynamic/riscv64/instruction-builder/builder.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

/**
 * Generate zero and sign flags based on the result in the given register
 * @param zf Destination register of zero flag, pass none_reg to skip
 * @param sf Destination register of sign flag, pass none_reg to skip
 */
void zero_sign_flag(InstructionBuilder &builder, TypedRegister &out, RegisterOperand zf, RegisterOperand sf)
{
	if (zf || sf) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (zf) {
				builder.seqz(zf, out); // ZF
			}
			if (sf) {
				builder.sltz(sf, out); // SF
			}
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input registers assuming the operation was an addition.
 * @param zf Destination register of zero flag, pass none_reg to skip
 * @param of Destination register of overflow flag, pass none_reg to skip
 * @param cf Destination register of carry flag, pass none_reg to skip
 * @param sf Destination register of sign flag, pass none_reg to skip
 */
inline void add_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, RegisterOperand zf, RegisterOperand of,
	RegisterOperand cf, RegisterOperand sf)
{
	if (of || cf || zf || sf) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (of) {
				extend_to_64(builder, lhs, lhs);
				extend_to_64(builder, rhs, rhs);

				RegisterOperand temp = builder.next_register();
				builder.sltz(temp, lhs);
				builder.slt(of, out, rhs);
				builder.xor_(of, of, temp); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (cf) {
				extend_to_64(builder, rhs, rhs);

				builder.sltu(cf, out, rhs); // CF (Allows typical x86 case of lhs==out) FIXME Assumes out!=rhs
			}
			zero_sign_flag(builder, out, zf, sf);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input register and immediate assuming the operation was
 * an addition.
 * @param zf Destination register of zero flag, pass none_reg to skip
 * @param of Destination register of overflow flag, pass none_reg to skip
 * @param cf Destination register of carry flag, pass none_reg to skip
 * @param sf Destination register of sign flag, pass none_reg to skip
 */
inline void addi_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, intptr_t imm, RegisterOperand zf, RegisterOperand of,
	RegisterOperand cf, RegisterOperand sf)
{
	if (of || cf || zf || sf) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (of) {
				extend_to_64(builder, lhs, lhs);

				builder.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm < 0) {
					builder.xori(of, of, 1); // Invert on negative
				}
			}
			if (cf) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(cf, out, lhs); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, zf, sf);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input registers assuming the operation was a
 * subtraction.
 * @param zf Destination register of zero flag, pass none_reg to skip
 * @param of Destination register of overflow flag, pass none_reg to skip
 * @param cf Destination register of carry flag, pass none_reg to skip
 * @param sf Destination register of sign flag, pass none_reg to skip
 */
inline void sub_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs, RegisterOperand zf, RegisterOperand of,
	RegisterOperand cf, RegisterOperand sf)
{
	if (of || cf || zf || sf) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (of) {
				extend_to_64(builder, lhs, lhs);
				extend_to_64(builder, rhs, rhs);

				RegisterOperand temp = builder.next_register();
				builder.sgtz(temp, rhs);
				builder.slt(of, out, lhs);
				builder.xor_(of, of, temp); // OF FIXME Assumes out!=lhs && out!=rhs
			}
			if (cf) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(cf, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, zf, sf);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate zero, overflow, carry and sign flags based on the result in the given register and the given input register and immediate assuming the operation was
 * a subtraction.
 * @param zf Destination register of zero flag, pass none_reg to skip
 * @param of Destination register of overflow flag, pass none_reg to skip
 * @param cf Destination register of carry flag, pass none_reg to skip
 * @param sf Destination register of sign flag, pass none_reg to skip
 */
inline void subi_flags(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, intptr_t imm, RegisterOperand zf, RegisterOperand of,
	RegisterOperand cf, RegisterOperand sf)
{
	if (of || cf || zf || sf) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			extend_to_64(builder, out, out);
			if (of) {
				extend_to_64(builder, lhs, lhs);

				builder.slt(of, out, lhs); // OF FIXME Assumes out!=lhs
				if (imm > 0) {
					builder.xori(of, of, 1); // Invert on positive
				}
			}
			if (cf) {
				extend_to_64(builder, lhs, lhs);

				builder.sltu(cf, lhs, out); // CF FIXME Assumes out!=lhs
			}
			zero_sign_flag(builder, out, zf, sf);
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}

/**
 * Generate overflow and carry flags based on the result in the given register and the given input register and immediate assuming the operation was a
 * multiplication.
 * @param of Destination register of overflow flag, pass none_reg to skip
 * @param cf Destination register of carry flag, pass none_reg to skip
 */
inline void mul_flags(InstructionBuilder &builder, TypedRegister &out, RegisterOperand of, RegisterOperand cf)
{
	RegisterOperand flag_reg = cf ?: of;

	if (flag_reg) {
		if (!out.type().is_vector() && out.type().is_integer()) {
			switch (out.type().element_width()) {
			case 128: {
				switch (out.type().element_type().type_class()) {
				case value_type_class::signed_integer:
					builder.srai(flag_reg, out.reg1(), 63);
					builder.xor_(flag_reg, flag_reg, out.reg2());
					builder.snez(flag_reg, flag_reg);
					break;
				case value_type_class::unsigned_integer: {
					builder.snez(flag_reg, out.reg2());
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
						builder.sextw(flag_reg, out);
					} else {
						builder.slli(flag_reg, out, 64 - (out.type().element_width()) / 2);
						builder.srai(flag_reg, out, 64 - (out.type().element_width()) / 2);
					}
					builder.xor_(flag_reg, flag_reg, out);
					builder.snez(flag_reg, flag_reg);
					break;
				case value_type_class::unsigned_integer:
					builder.srli(flag_reg, out, out.type().element_width() / 2);
					builder.snez(flag_reg, flag_reg);
					break;
				default:
					throw std::runtime_error("Unsupported value type for multiply");
				}

			} break;
			default:
				throw std::runtime_error("Unsupported width for sub immediate");
			}
			if (cf && of) {
				builder.mv(of, flag_reg);
			}
		} else {
			throw std::runtime_error("not implemented");
		}
	}
}
