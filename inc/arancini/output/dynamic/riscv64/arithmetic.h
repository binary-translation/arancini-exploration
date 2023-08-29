/**
 * All of these implementations assume that the widths of the input registers are at least as big as the intended output.
 */
#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>

inline void add(Assembler &assembler, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
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
				assembler.add(out, lhs, rhs);
				break;
			case 32:
				assembler.addw(out, lhs, rhs);
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

				// Add upper half of lower register
				assembler.srli(CF, src_reg1, 32);
				assembler.srli(OF, src_reg2, 32);
				assembler.add(OF, OF, CF);
				assembler.slli(OF, OF, 32);

				// Add lower half of lower register
				assembler.add(out_reg, src_reg1, src_reg2);
				assembler.slli(out_reg, out_reg, 32);
				assembler.srli(out_reg, out_reg, 32);

				assembler.or_(out_reg, out_reg, OF);

				Register src_reg12 = lhs.reg2();
				Register src_reg22 = rhs.reg2();
				Register out_reg2 = out.reg2();

				// Add upper half of upper register
				assembler.srli(CF, src_reg12, 32);
				assembler.srli(OF, src_reg22, 32);
				assembler.add(OF, OF, CF);
				assembler.slli(OF, OF, 32);

				// Add lower half of upper register
				assembler.add(out_reg2, src_reg12, src_reg22);
				assembler.slli(out_reg2, out_reg2, 32);
				assembler.srli(out_reg2, out_reg2, 32);

				assembler.or_(out_reg2, out_reg2, OF);
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

inline void addi(Assembler &assembler, TypedRegister &out, const TypedRegister &lhs, intptr_t imm)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 8:
		case 16:
		case 64:
			assembler.addi(out, lhs, imm);
			break;
		case 32:
			assembler.addiw(out, lhs, imm);
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

inline void sub(Assembler &assembler, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 8:
		case 16:
		case 64:
			assembler.sub(out, lhs, rhs);
			break;
		case 32:
			assembler.subw(out, lhs, rhs);
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

inline void xor_(Assembler &assembler, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
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
			assembler.xor_(out, lhs, rhs);
			break;
		case 128:
			assembler.xor_(out.reg1(), lhs.reg1(), rhs.reg1());
			assembler.xor_(out.reg2(), lhs.reg2(), rhs.reg2());
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void mul(Assembler &assembler, TypedRegister &out, const TypedRegister &lhs, const TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().element_width()) {
		case 128: {
			// Split calculation
			assembler.mul(out.reg1(), lhs, rhs);
			switch (out.type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.mulh(out.reg2(), lhs, rhs);
				break;
			case value_type_class::unsigned_integer:
				assembler.mulhu(out.reg2(), lhs, rhs);
				break;
			default:
				// should not happen
				throw std::runtime_error("not implemented");
			}
		} break;

		case 64:
		case 32:
		case 16:
			assembler.mul(out, lhs, rhs); // Assumes proper signed/unsigned extension from 32/16/8 bits
			break;
		default:
			throw std::runtime_error("not implemented");
		}
	} else {
		throw std::runtime_error("not implemented");
	}
}

inline void div(Assembler &assembler, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().type_class()) {

		case value_type_class::signed_integer:
			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				assembler.div(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				assembler.div(out, lhs, rhs);
				break;
			case 16:
				fixup(assembler, lhs, lhs, value_type::s32());
				fixup(assembler, rhs, rhs, value_type::s32());
				[[fallthrough]];
				// fallthrough (use 32 bit div because it might be faster on hardware)

				// Almost sure that this will mean the result will be correctly sign extended to 64 bit after the divw
				// Only the overflow case of MIN_INT16 / -1 will not be correct but that should crash anyway so not an issue
			case 32:
				assembler.divw(out, lhs, rhs);
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
				assembler.divu(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				assembler.divu(out, lhs, rhs);
				break;
			case 16:
				fixup(assembler, lhs, lhs, value_type::u32());
				fixup(assembler, rhs, rhs, value_type::u32());
				[[fallthrough]];
				// fallthrough (use 32 bit div because it might be faster on hardware)
			case 32:
				assembler.divuw(out, lhs, rhs);
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

inline void mod(Assembler &assembler, TypedRegister &out, TypedRegister &lhs, TypedRegister &rhs)
{
	if (!out.type().is_vector() && out.type().is_integer()) {
		switch (out.type().type_class()) {

		case value_type_class::signed_integer:
			switch (out.type().element_width()) {
			case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
				assembler.rem(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				assembler.rem(out, lhs, rhs);
				break;
			case 16:
				fixup(assembler, lhs, lhs, value_type::s32());
				fixup(assembler, rhs, rhs, value_type::s32());
				[[fallthrough]];
				// fallthrough (use 32 bit rem because it might be faster on hardware)
			case 32:
				assembler.remw(out, lhs, rhs);
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
				assembler.remu(out.reg1(), lhs.reg1(), rhs.reg1());
				break;
			case 64:
				assembler.remu(out, lhs, rhs);
				break;
			case 16:
				fixup(assembler, lhs, lhs, value_type::u32());
				fixup(assembler, rhs, rhs, value_type::u32());
				[[fallthrough]];
				// fallthrough (use 32 bit rem because it might be faster on hardware)

				// Almost sure that this will mean the result will be correctly sign extended to 64 bit after the remw
				// Only the overflow case of MIN_INT16 % -1 will not be correct but that should crash anyway so not an issue
			case 32:
				assembler.remuw(out, lhs, rhs);
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
