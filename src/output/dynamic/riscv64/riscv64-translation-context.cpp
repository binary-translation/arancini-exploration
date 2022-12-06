#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/machineCode/constants_riscv.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

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
		Register regVal = materialise(value.owner());
		assembler.sd(regVal, { FP, static_cast<intptr_t>(n2->regoff()) });

	} break;
	case node_kinds::constant: {
		return materialiseConstant((int64_t)((constant_node *)n)->const_val_i());
	}
	default:
		throw std::runtime_error("unsupported node");
	}
	return ZERO;
}
Register riscv64_translation_context::materialiseConstant(int64_t imm)
{
	//Optimizations with left or right shift at the end not implemented (for constants with trailing or leading zeroes)

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
		auto val = (int64_t)((uint64_t)imm - (uint64_t)(int64_t)immLo12); //Get lower 12 bits out of imm
		int shiftAmnt = 0;
		if (!Utils::IsInt(32, val)) { //Might still not fit as LUI with unsigned add
			shiftAmnt = __builtin_ctzll(val);
			val >>= shiftAmnt;
			if (shiftAmnt > 12 && !IsITypeImm(val)&& Utils::IsInt(32, val<<12)) { //Does not fit into 12 bits but can fit into LUI U-immediate with proper shift
				val <<= 12;
				shiftAmnt -= 12;
			}
		}

		materialiseConstant(val);

		if (shiftAmnt) {
			assembler.slli(outReg, outReg, shiftAmnt);
		}

		if (immLo12) {
			assembler.addi(outReg, outReg, immLo12);
		}

	}
	return outReg;
}
