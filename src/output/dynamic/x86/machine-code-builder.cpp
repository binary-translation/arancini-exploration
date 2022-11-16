#include <arancini/output/dynamic/x86/machine-code-builder.h>
#include <iostream>

using namespace arancini::output::dynamic::x86;

static const char *mnemonics[] = { "mov", "and", "or", "xor", "add", "sub", "setz", "seto", "setc", "sets" };

void instruction::dump(std::ostream &os) const
{
	if ((int)opcode > (sizeof(mnemonics) / sizeof(mnemonics[0]))) {
		os << "???";
	} else {
		os << mnemonics[(int)opcode];
	}

	for (int i = 0; i < sizeof(operands) / sizeof(operands[0]); i++) {
		if (operands[i].kind == operand_kind::none) {
			continue;
		}

		if (i > 0) {
			os << ", ";

		} else {
			os << " ";
		}

		operands[i].dump(os);
	}

	os << std::endl;
}

void operand::dump(std::ostream &os) const
{
	switch (kind) {
	case operand_kind::none:
		break;

	case operand_kind::reg:
		reg_i.rr.dump(os);
		break;

	case operand_kind::mem:
		if (mem_i.displacement) {
			os << std::dec << mem_i.displacement;
		}

		os << "(";
		mem_i.base.dump(os);
		os << ")";
		break;

	case operand_kind::immediate:
		os << "$0x" << std::hex << imm_i.val;
		break;
	}

	os << ":" << std::dec << width;
}

static const char *regnames[] = { "rax", "rcx", "rdx", "rbx", "rsi", "rdi", "rsp", "rbp" };

void regref::dump(std::ostream &os) const
{
	os << "%";

	switch (kind) {
	case regref_kind::phys:
		os << regnames[(int)preg_i];
		break;

	case regref_kind::virt:
		os << "V" << std::dec << vreg_i;
		break;
	}
}
