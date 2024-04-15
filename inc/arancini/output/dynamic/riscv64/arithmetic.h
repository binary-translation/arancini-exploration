/**
 * All of these implementations assume that the widths of the input registers are at least as big as the intended output.
 */
#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/instruction-builder/builder.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void add(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	switch (out.type().type_class()) {

	case value_type_class::signed_integer:
	case value_type_class::unsigned_integer:

		switch (out.type().nr_elements()) {
		case 1: {
			switch (out.type().element_width()) {
			case 8:
			case 16:
			case 64:
				builder.add(out, lhs, rhs);
				break;
			case 32:
				builder.addw(out, lhs, rhs);
				out.set_actual_width();
				out.set_type(value_type::u64());
				break;
			default:
				throw std::runtime_error("not implemented");
			}
		} break;

		case 4:
			if (out.type().element_width() == 32) {
				Register src_reg1 = lhs.reg1();
				Register src_reg2 = rhs.reg1();
				Register out_reg = out.reg1();

				RegisterOperand temp1 = builder.next_register();
				RegisterOperand temp2 = builder.next_register();

				// Add upper half of lower register
				builder.srli(temp1, src_reg1, 32);
				builder.srli(temp2, src_reg2, 32);
				builder.add(temp2, temp2, temp1);
				builder.slli(temp2, temp2, 32);

				// Add lower half of lower register
				builder.add(out_reg, src_reg1, src_reg2);
				builder.slli(out_reg, out_reg, 32);
				builder.srli(out_reg, out_reg, 32);

				builder.or_(out_reg, out_reg, temp2);

				Register src_reg12 = lhs.reg2();
				Register src_reg22 = rhs.reg2();
				Register out_reg2 = out.reg2();

				// Add upper half of upper register
				builder.srli(temp1, src_reg12, 32);
				builder.srli(temp2, src_reg22, 32);
				builder.add(temp2, temp2, temp1);
				builder.slli(temp2, temp2, 32);

				// Add lower half of upper register
				builder.add(out_reg2, src_reg12, src_reg22);
				builder.slli(out_reg2, out_reg2, 32);
				builder.srli(out_reg2, out_reg2, 32);

				builder.or_(out_reg2, out_reg2, temp2);
			}
			break;
		case 16:
			if (out.type().element_width() == 8) {
				RegisterOperand mask1 = builder.next_register();
				RegisterOperand mask2 = builder.next_register();

				gen_constant(builder, 0x7F7F7F7F7F7F7F7Full, mask1);

				builder.neg(mask2, mask1);

				RegisterOperand temp1 = builder.next_register();
				RegisterOperand temp2 = builder.next_register();

				Register src_reg1 = lhs.reg1();
				Register src_reg2 = rhs.reg1();
				Register out_reg = out.reg1();

				builder.and_(temp1, src_reg1, mask1);
				builder.and_(temp2, src_reg2, mask1);
				builder.add(temp1, temp1, temp2); // Add without high bits


				builder.xor_(out_reg, src_reg1, src_reg2); // carryless addition
				builder.and_(out_reg, out_reg, mask2); // Just high bits

				builder.xor_(out_reg, out_reg, temp1);

				Register src_reg12 = lhs.reg2();
				Register src_reg22 = rhs.reg2();
				Register out_reg2 = out.reg2();

				builder.and_(temp1, src_reg12, mask1);
				builder.and_(temp2, src_reg22, mask1);
				builder.add(temp1, temp1, temp2); // Add without high bits


				builder.xor_(out_reg2, src_reg12, src_reg22); // carryless addition
				builder.and_(out_reg2, out_reg2, mask2); // Just high bits

				builder.xor_(out_reg2, out_reg2, temp1);
			}
			break;
		default:
			throw std::runtime_error("not implemented");
		}
		break;
	default:
		throw std::runtime_error("not implemented");
	}
}

inline void addi(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &lhs, intptr_t imm)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 8:
		case 16:
		case 64:
			builder.addi(out, lhs, imm);
			break;
		case 32:
			builder.addiw(out, lhs, imm);
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

inline void sub(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	switch (out.type().type_class()) {

	case value_type_class::signed_integer:
	case value_type_class::unsigned_integer:
		switch (out.type().nr_elements()) {
		case 1: {
			switch (out.type().element_width()) {
			case 8:
			case 16:
			case 64:
				builder.sub(out, lhs, rhs);
				break;
			case 32:
				builder.subw(out, lhs, rhs);
				out.set_actual_width();
				out.set_type(value_type::u64());
				break;
			default:
				throw std::runtime_error("not implemented");
			}
		} break;

		case 4:
			if (out.type().element_width() == 32) {
				Register src_reg1 = lhs.reg1();
				Register src_reg2 = rhs.reg1();
				Register out_reg = out.reg1();

				RegisterOperand temp1 = builder.next_register();
				RegisterOperand temp2 = builder.next_register();

				// Add upper half of lower register
				builder.srli(temp1, src_reg1, 32);
				builder.srli(temp2, src_reg2, 32);
				builder.sub(temp2, temp2, temp1);
				builder.slli(temp2, temp2, 32);

				// Add lower half of lower register
				builder.sub(out_reg, src_reg1, src_reg2);
				builder.slli(out_reg, out_reg, 32);
				builder.srli(out_reg, out_reg, 32);

				builder.or_(out_reg, out_reg, temp2);

				Register src_reg12 = lhs.reg2();
				Register src_reg22 = rhs.reg2();
				Register out_reg2 = out.reg2();

				// Add upper half of upper register
				builder.srli(temp1, src_reg12, 32);
				builder.srli(temp2, src_reg22, 32);
				builder.sub(temp2, temp2, temp1);
				builder.slli(temp2, temp2, 32);

				// Add lower half of upper register
				builder.sub(out_reg2, src_reg12, src_reg22);
				builder.slli(out_reg2, out_reg2, 32);
				builder.srli(out_reg2, out_reg2, 32);

				builder.or_(out_reg2, out_reg2, temp2);
			}
			break;
		default:
			throw std::runtime_error("not implemented");
		}
		break;
	default:
		throw std::runtime_error("not implemented");
	}
}

inline void xor_(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 1:
		case 8:
		case 16:
		case 32:
			// In-types might be wider than out-type (but xor preserves extension, so out accurate to narrower of in)

			out.set_actual_width();
			out.set_type(get_minimal_type(lhs, rhs));
			[[fallthrough]];
		case 64:
			builder.xor_(out, lhs, rhs);
			break;
		case 128:
			builder.xor_(out.reg1(), lhs.reg1(), rhs.reg1());
			builder.xor_(out.reg2(), lhs.reg2(), rhs.reg2());
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void mul(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 128: {
			// Split calculation
			builder.mul(out.reg1(), lhs, rhs);
			switch (out.type().element_type().type_class()) {

			case value_type_class::signed_integer:
				builder.mulh(out.reg2(), lhs, rhs);
				break;
			case value_type_class::unsigned_integer:
				builder.mulhu(out.reg2(), lhs, rhs);
				break;
			default:
				// should not happen
				throw std::runtime_error("not implemented");
			}
		} break;

		case 64:
		case 32:
		case 16:
			builder.mul(out, lhs, rhs); // Assumes proper signed/unsigned extension from 32/16/8 bits
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void div(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().type_class()) {

		case value_type_class::signed_integer:
			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				builder.div(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				builder.div(out, lhs, rhs);
				break;
			case 16:
				fixup(builder, lhs, lhs, value_type::s32());
				fixup(builder, rhs, rhs, value_type::s32());
				[[fallthrough]];
				// fallthrough (use 32 bit div because it might be faster on hardware)

				// Almost sure that this will mean the result will be correctly sign extended to 64 bit after the divw
				// Only the overflow case of MIN_INT16 / -1 will not be correct but that should crash anyway so not an issue
			case 32:
				builder.divw(out, lhs, rhs);
				out.set_actual_width();
				out.set_type(value_type::s64());
				break;
			default:
				throw std::runtime_error("not implemented");
			}
			break;
		case value_type_class::unsigned_integer:

			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				builder.divu(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				builder.divu(out, lhs, rhs);
				break;
			case 16:
				fixup(builder, lhs, lhs, value_type::u32());
				fixup(builder, rhs, rhs, value_type::u32());
				[[fallthrough]];
				// fallthrough (use 32 bit div because it might be faster on hardware)
			case 32:
				builder.divuw(out, lhs, rhs);
				break;
			default:
				throw std::runtime_error("not implemented");
			}
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void mod(InstructionBuilder &builder, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().type_class()) {

		case value_type_class::signed_integer:
			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				builder.rem(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				builder.rem(out, lhs, rhs);
				break;
			case 16:
				fixup(builder, lhs, lhs, value_type::s32());
				fixup(builder, rhs, rhs, value_type::s32());
				[[fallthrough]];
				// fallthrough (use 32 bit rem because it might be faster on hardware)
			case 32:
				builder.remw(out, lhs, rhs);
				out.set_actual_width();
				out.set_type(value_type::s64());
				break;
			default:
				throw std::runtime_error("not implemented");
			}
			break;
		case value_type_class::unsigned_integer:

			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				builder.remu(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				builder.remu(out, lhs, rhs);
				break;
			case 16:
				fixup(builder, lhs, lhs, value_type::u32());
				fixup(builder, rhs, rhs, value_type::u32());
				[[fallthrough]];
				// fallthrough (use 32 bit rem because it might be faster on hardware)

				// Almost sure that this will mean the result will be correctly sign extended to 64 bit after the remw
				// Only the overflow case of MIN_INT16 % -1 will not be correct but that should crash anyway so not an issue
			case 32:
				builder.remuw(out, lhs, rhs);
				break;
			default:
				throw std::runtime_error("not implemented");
			}
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void not_(InstructionBuilder &builder, TypedRegister &out, const TypedRegister &src)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		if (is_flag_t(out.type())) {
			builder.xori(out, src, 1);
			out.set_type(value_type::u64());
		} else if (is_gpr_t(out.type())) {
			builder.not_(out, src);
			out.set_actual_width();
			out.set_type(src.type());
		} else {
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}
