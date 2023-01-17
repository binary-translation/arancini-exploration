#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

/**
 * All values are always held sign extended to 64 bits in RISCV registers for simple flag calculation
 * Translations assumes FP holds pointer to CPU state.
 * Flags are always stored in registers S8 (ZF), S9 (CF), S10 (OF), S11(SF)
 */
constexpr Register ZF = S8;
constexpr Register CF = S9;
constexpr Register OF = S10;
constexpr Register SF = S11;

void riscv64_translation_context::begin_block() { }
void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void riscv64_translation_context::end_instruction() { }
void riscv64_translation_context::end_block() { }
void riscv64_translation_context::lower(ir::node *n) { materialise(n); }
Register riscv64_translation_context::materialise(const node *n)
{
	switch (n->kind()) {
	case node_kinds::write_reg: {

		auto n2 = (write_reg_node *)n;

		port &value = n2->value();
		if (value.type().width() == 1) { // Flags
			// TODO
		} else if (value.type().width() == 64) { // GPR

			Register regVal = materialise(value.owner());
			assembler.sd(regVal, { FP, static_cast<intptr_t>(n2->regoff()) });
		} else {
			throw std::runtime_error("Unsupported width on register write");
		}

	} break;
	case node_kinds::constant: {
		return materialise_constant((int64_t)((constant_node *)n)->const_val_i());
	}
	case node_kinds::binary_arith: {
		return materialise_binary_arith((binary_arith_node *)n);
	}

	case node_kinds::unary_arith: {
		auto n2 = (unary_arith_node *)n;
		Register outReg = T0;

		Register srcReg1 = materialise(n2->lhs().owner());
		switch (n2->op()) {

		case unary_arith_op::bnot:
			assembler.not_(outReg, srcReg1);
			break;
		default:
			throw std::runtime_error("unsupported binary arithmetic operation");
		}

	} break;
	case node_kinds::ternary_arith: {
		return materialise_ternary_arith((ternary_arith_node *)n);
	}

	default:
		throw std::runtime_error("unsupported node");
	}
	return ZERO;
}
Register riscv64_translation_context::materialise_ternary_arith(ternary_arith_node *n2)
{
	Register outReg = T0;

	Register srcReg1 = materialise(n2->lhs().owner());
	Register srcReg2 = materialise(n2->rhs().owner());
	Register srcReg3 = materialise(n2->top().owner());
	switch (n2->op()) {
		// TODO Immediate handling

	case ternary_arith_op::adc:
		// Temporary: Add carry in
		switch (n2->val().type().width()) {
		case 64:
			assembler.add(ZF, srcReg2, srcReg3);
			break;
		case 32:
			assembler.addw(ZF, srcReg2, srcReg3);
			break;
		case 8:
		case 16:
			assembler.add(ZF, srcReg2, srcReg3);
			assembler.slli(ZF, ZF, 64 - n2->val().type().width());
			assembler.srai(ZF, ZF, 64 - n2->val().type().width());
			break;
		}

		assembler.slt(OF, ZF, srcReg2); // Temporary overflow
		assembler.sltu(CF, ZF, srcReg2); // Temporary carry

		// Normal add
		switch (n2->val().type().width()) {
		case 64:
			assembler.add(outReg, srcReg1, ZF);
			break;
		case 32:
			assembler.addw(outReg, srcReg1, ZF);
			break;
		case 8:
		case 16:
			assembler.add(outReg, srcReg1, ZF);
			assembler.slli(outReg, outReg, 64 - n2->val().type().width());
			assembler.srai(outReg, outReg, 64 - n2->val().type().width());
			break;
		}

		assembler.sltu(SF, outReg, ZF); // Normal carry out
		assembler.or_(CF, CF, SF); // Total carry out

		assembler.sltz(SF, srcReg1);
		assembler.slt(ZF, outReg, ZF);
		assembler.xor_(ZF, ZF, SF); // Normal overflow out
		assembler.xor_(OF, OF, ZF); // Total overflow out

		break;
	case ternary_arith_op::sbb:
		// Temporary: Add carry in
		switch (n2->val().type().width()) {
		case 64:
			assembler.add(ZF, srcReg2, srcReg3);
			break;
		case 32:
			assembler.addw(ZF, srcReg2, srcReg3);
			break;
		case 8:
		case 16:
			assembler.add(ZF, srcReg2, srcReg3);
			assembler.slli(ZF, ZF, 64 - n2->val().type().width());
			assembler.srai(ZF, ZF, 64 - n2->val().type().width());
			break;
		}

		assembler.slt(OF, ZF, srcReg2); // Temporary overflow
		assembler.sltu(CF, ZF, srcReg2); // Temporary carry

		switch (n2->val().type().width()) {
		case 64:
			assembler.sub(outReg, srcReg1, ZF);
		case 32:
			assembler.subw(outReg, srcReg1, ZF);
		case 16:
			assembler.sub(outReg, srcReg1, ZF);
			assembler.slli(outReg, outReg, 64 - n2->val().type().width());
			assembler.srai(outReg, outReg, 64 - n2->val().type().width());
			break;
		}

		assembler.sltu(SF, srcReg1, outReg); // Normal carry out
		assembler.or_(CF, CF, SF); // Total carry out

		assembler.sgtz(SF, ZF);
		assembler.slt(ZF, outReg, srcReg1);
		assembler.xor_(ZF, ZF, SF); // Normal overflow out
		assembler.xor_(OF, OF, ZF); // Total overflow out

		break;
	default:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	assembler.seqz(ZF, outReg); // ZF
	assembler.sltz(SF, outReg); // SF

	return outReg;
}
Register riscv64_translation_context::materialise_binary_arith(binary_arith_node *n2)
{
	Register outReg = T0;
	Register outReg2 = T1;

	Register srcReg1 = materialise(n2->lhs().owner());

	if (n2->rhs().owner()->kind() == node_kinds::constant) {
		// Could also work for LHS except sub
		// TODO Probably incorrect to just cast to signed 64bit
		auto imm = (intptr_t)((constant_node *)(n2->rhs().owner()))->const_val_i();
		if (imm)

			if (IsITypeImm(imm)) {
				switch (n2->op()) {

				case binary_arith_op::sub:
					if (imm == -2048) { // Can happen with inversion
						goto standardPath;
					}
					switch (n2->val().type().width()) {
					case 64:
						assembler.addi(outReg, srcReg1, -imm);
						break;
					case 32:
						assembler.addiw(outReg, srcReg1, -imm);
						break;
					case 8:
					case 16:
						assembler.addi(outReg, srcReg1, -imm);
						assembler.slli(outReg, outReg, 64 - n2->val().type().width());
						assembler.srai(outReg, outReg, 64 - n2->val().type().width());
						break;
					}
					assembler.sltu(CF, srcReg1, outReg); // CF FIXME Assumes outReg!=srcReg1
					assembler.slt(OF, outReg, srcReg1); // OF FIXME Assumes outReg!=srcReg1
					if (imm > 0) {
						assembler.xori(OF, OF, 1); // Invert on positive
					}
					break;

				case binary_arith_op::add:

					switch (n2->val().type().width()) {
					case 64:
						assembler.addi(outReg, srcReg1, imm);
						break;
					case 32:
						assembler.addiw(outReg, srcReg1, -imm);
						break;
					case 8:
					case 16:
						assembler.addi(outReg, srcReg1, imm);
						assembler.slli(outReg, outReg, 64 - n2->val().type().width());
						assembler.srai(outReg, outReg, 64 - n2->val().type().width());
						break;
					}

					assembler.sltu(CF, outReg, srcReg1); // CF FIXME Assumes outReg!=srcReg1
					assembler.slt(OF, outReg, srcReg1); // OF FIXME Assumes outReg!=srcReg1
					if (imm < 0) {
						assembler.xori(OF, OF, 1); // Invert on negative
					}

					break;
				// Binary operations preserve sign extension
				case binary_arith_op::band:
					assembler.andi(outReg, srcReg1, imm);
					break;
				case binary_arith_op::bor:
					assembler.ori(outReg, srcReg1, imm);
					break;
				case binary_arith_op::bxor:
					assembler.xori(outReg, srcReg1, imm);
					break;
				default:
					// No-op Go to standard path
					goto standardPath;
				}

				assembler.seqz(ZF, outReg); // ZF
				assembler.sltz(SF, outReg); // SF

				return outReg;
			}
	}

standardPath:
	Register srcReg2 = materialise(n2->rhs().owner());
	switch (n2->op()) {

	case binary_arith_op::add:
		switch (n2->val().type().width()) {
		case 64:
			assembler.add(outReg, srcReg1, srcReg2);
			break;
		case 32:
			assembler.addw(outReg, srcReg1, srcReg2);
			break;
		case 8:
		case 16:
			assembler.add(outReg, srcReg1, srcReg2);
			assembler.slli(outReg, outReg, 64 - n2->val().type().width());
			assembler.srai(outReg, outReg, 64 - n2->val().type().width());
			break;
		}

		assembler.sltz(CF, srcReg1);
		assembler.slt(OF, outReg, srcReg2);
		assembler.xor_(OF, OF, CF); // OF FIXME Assumes outReg!=srcReg1 && outReg!=srcReg2

		assembler.sltu(CF, outReg, srcReg2); // CF (Allows typical x86 case of regSrc1==outReg) FIXME Assumes outReg!=srcReg2
		break;

	case binary_arith_op::sub:
		switch (n2->val().type().width()) {
		case 64:
			assembler.sub(outReg, srcReg1, srcReg2);
			break;
		case 32:
			assembler.subw(outReg, srcReg1, srcReg2);
			break;
		case 8:
		case 16:
			assembler.sub(outReg, srcReg1, srcReg2);
			assembler.slli(outReg, outReg, 64 - n2->val().type().width());
			assembler.srai(outReg, outReg, 64 - n2->val().type().width());
			break;
		}
		assembler.sgtz(CF, srcReg2);
		assembler.slt(OF, outReg, srcReg1);
		assembler.xor_(OF, OF, CF); // OF FIXME Assumes outReg!=srcReg1 && outReg!=srcReg2

		assembler.sltu(CF, srcReg1, outReg); // CF FIXME Assumes outReg!=srcReg1
		break;

	// Binary operations preserve sign extension
	case binary_arith_op::band:
		assembler.and_(outReg, srcReg1, srcReg2);
		break;
	case binary_arith_op::bor:
		assembler.or_(outReg, srcReg1, srcReg2);
		break;
	case binary_arith_op::bxor:
		assembler.xor_(outReg, srcReg1, srcReg2);
		break;

	case binary_arith_op::mul:
		switch (n2->val().type().width()) {
		case 128:
			// Split calculation
			assembler.mul(outReg, srcReg1, srcReg2);
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.mulh(outReg2, srcReg1, srcReg2);
				assembler.srai(CF, outReg, 64);
				assembler.xor_(CF, CF, outReg2);
				assembler.snez(CF, CF);
				break;
			case value_type_class::unsigned_integer:
				assembler.mulhu(outReg2, srcReg1, srcReg2);
				assembler.snez(CF, outReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for multiply");
			}
			assembler.mv(OF, CF);
			break;

		case 64:
		case 32:
		case 16:
			assembler.mul(outReg, srcReg1, srcReg2); // Assumes proper signed/unsigned extension from 32/16/8 bits

			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				if (n2->val().type().width() == 64) {
					assembler.sextw(CF, outReg);
				} else {
					assembler.slli(CF, outReg, 64 - (n2->val().type().width()) / 2);
					assembler.srai(CF, outReg, 64 - (n2->val().type().width()) / 2);
				}
				assembler.xor_(CF, CF, outReg);
				assembler.snez(CF, CF);
				break;
			case value_type_class::unsigned_integer:
				assembler.srli(CF, outReg, n2->val().type().width() / 2);
				assembler.snez(CF, outReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for multiply");
			}
			assembler.mv(OF, CF);
			break;
		}
	case binary_arith_op::div:
		switch (n2->val().type().width()) {
		case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
		case 64:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.div(outReg, srcReg1, srcReg2);
				break;
			case value_type_class::unsigned_integer:
				assembler.divu(outReg, srcReg1, srcReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 32:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.divw(outReg, srcReg1, srcReg2);
				break;
			case value_type_class::unsigned_integer:
				assembler.divuw(outReg, srcReg1, srcReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 16:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.divw(outReg, srcReg1, srcReg2);
				assembler.slli(outReg, outReg, 48);
				assembler.srai(outReg, outReg, 48);
				break;
			case value_type_class::unsigned_integer:
				assembler.divuw(outReg, srcReg1, srcReg2);
				assembler.slli(outReg, outReg, 48);
				assembler.srli(outReg, outReg, 48);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		}

	case binary_arith_op::mod:
		switch (n2->val().type().width()) {
		case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
		case 64:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.rem(outReg, srcReg1, srcReg2);
				break;
			case value_type_class::unsigned_integer:
				assembler.remu(outReg, srcReg1, srcReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 32:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.remw(outReg, srcReg1, srcReg2);
				break;
			case value_type_class::unsigned_integer:
				assembler.remuw(outReg, srcReg1, srcReg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 16:
			switch (n2->val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler.remw(outReg, srcReg1, srcReg2);
				assembler.slli(outReg, outReg, 48);
				assembler.srai(outReg, outReg, 48);
				break;
			case value_type_class::unsigned_integer:
				assembler.remuw(outReg, srcReg1, srcReg2);
				assembler.slli(outReg, outReg, 48);
				assembler.srli(outReg, outReg, 48);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		}

	case binary_arith_op::cmpeq:
		assembler.xor_(outReg, srcReg1, srcReg2);
		assembler.seqz(outReg, outReg);
	case binary_arith_op::cmpne:
		assembler.xor_(outReg, srcReg1, srcReg2);
		assembler.snez(outReg, outReg);
	case binary_arith_op::cmpgt:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	// TODO those should only be set on add, sub, xor, or, and
	assembler.seqz(ZF, outReg); // ZF
	assembler.sltz(SF, outReg); // SF

	return outReg;
}
Register riscv64_translation_context::materialise_constant(int64_t imm)
{
	// Optimizations with left or right shift at the end not implemented (for constants with trailing or leading zeroes)

	if (imm == 0) {
		return ZERO;
	}
	Register outReg = A0;
	auto immLo32 = (int32_t)imm;
	auto immHi32 = imm >> 32 << 32;
	auto immLo12 = immLo32 << (32 - 12) >> (32 - 12); // sign extend lower 12 bit
	if (immHi32 == 0) {
		int32_t imm32Hi20 = (immLo32 - immLo12);
		if (imm32Hi20 != 0) {
			assembler.lui(outReg, imm32Hi20);
			if (immLo12) {
				assembler.addiw(outReg, outReg, immLo12);
			}
		} else {
			assembler.li(outReg, imm);
		}

	} else {
		auto val = (int64_t)((uint64_t)imm - (uint64_t)(int64_t)immLo12); // Get lower 12 bits out of imm
		int shiftAmnt = 0;
		if (!Utils::IsInt(32, val)) { // Might still not fit as LUI with unsigned add
			shiftAmnt = __builtin_ctzll(val);
			val >>= shiftAmnt;
			if (shiftAmnt > 12 && !IsITypeImm(val)
				&& Utils::IsInt(32, val << 12)) { // Does not fit into 12 bits but can fit into LUI U-immediate with proper shift
				val <<= 12;
				shiftAmnt -= 12;
			}
		}

		materialise_constant(val);

		if (shiftAmnt) {
			assembler.slli(outReg, outReg, shiftAmnt);
		}

		if (immLo12) {
			assembler.addi(outReg, outReg, immLo12);
		}
	}
	return outReg;
}
