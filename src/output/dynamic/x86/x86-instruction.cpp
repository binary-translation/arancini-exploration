#include <arancini/output/dynamic/x86/x86-instruction.h>
#include <fadec-enc.h>

using namespace arancini::output::dynamic::x86;

void x86_instruction::emit(machine_code_writer &writer) const
{
	if (raw_opcode == 0) {
		return;
	}

	int rc;

	uint8_t buf[16];
	uint8_t *cur = buf;

	switch (opform) {
	case x86_opform::OF_NONE:
		rc = fe_enc64(&cur, raw_opcode);
		break;

	case x86_opform::OF_R8:
	case x86_opform::OF_R16:
	case x86_opform::OF_R32:
	case x86_opform::OF_R64:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		rc = fe_enc64(&cur, raw_opcode, (FeReg)operands[0].pregop.regname);
		break;

	case x86_opform::OF_M8:
	case x86_opform::OF_M16:
	case x86_opform::OF_M32:
	case x86_opform::OF_M64:
		if (!operands[0].is_mem()) {
			throw std::runtime_error("expected mem operand 0");
		}

		if (operands[0].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 0");
		}

		rc = fe_enc64(&cur, raw_opcode, FE_MEM((FeReg)operands[0].memop.pbase, 0, 0, operands[0].memop.displacement));
		break;

	case x86_opform::OF_R8_R8:
	case x86_opform::OF_R16_R16:
	case x86_opform::OF_R32_R32:
	case x86_opform::OF_R64_R64:
	case x86_opform::OF_R64_R32:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_preg()) {
			throw std::runtime_error("expected preg operand 1");
		}

		rc = fe_enc64(&cur, raw_opcode, (FeReg)operands[0].pregop.regname, (FeReg)operands[1].pregop.regname);
		break;

	case x86_opform::OF_R8_M8:
	case x86_opform::OF_R16_M16:
	case x86_opform::OF_R32_M32:
	case x86_opform::OF_R64_M64: {
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_mem()) {
			throw std::runtime_error("expected mem operand 1");
		}

		if (operands[1].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 1");
		}

		unsigned long overridden_opcode = raw_opcode;

		switch (operands[1].memop.seg) {
		case x86_register_names::FS:
			overridden_opcode |= FE_SEG(FE_FS);
			break;
		case x86_register_names::GS:
			overridden_opcode |= FE_SEG(FE_GS);
			break;
		default:
			break;
		}

		rc = fe_enc64(&cur, overridden_opcode, (FeReg)operands[0].pregop.regname, FE_MEM((FeReg)operands[1].memop.pbase, 0, 0, operands[1].memop.displacement));
		break;
	}

	case x86_opform::OF_M8_R8:
	case x86_opform::OF_M16_R16:
	case x86_opform::OF_M32_R32:
	case x86_opform::OF_M64_R64: {
		if (!operands[0].is_mem()) {
			throw std::runtime_error("expected mem operand 0");
		}

		if (operands[0].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 0");
		}

		unsigned long overridden_opcode = raw_opcode;

		switch (operands[0].memop.seg) {
		case x86_register_names::FS:
			overridden_opcode |= FE_SEG(FE_FS);
			break;
		case x86_register_names::GS:
			overridden_opcode |= FE_SEG(FE_GS);
			break;
		default:
			break;
		}

		if (!operands[1].is_preg()) {
			throw std::runtime_error("expected preg operand 1");
		}

		rc = fe_enc64(&cur, overridden_opcode, FE_MEM((FeReg)operands[0].memop.pbase, 0, 0, operands[0].memop.displacement), (FeReg)operands[1].pregop.regname);
		break;
	}

	case x86_opform::OF_M8_I8:
	case x86_opform::OF_M16_I16:
	case x86_opform::OF_M32_I32:
	case x86_opform::OF_M64_I64: {
		if (!operands[0].is_mem()) {
			throw std::runtime_error("expected mem operand 0");
		}

		if (operands[0].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 0");
		}

		unsigned long overridden_opcode = raw_opcode;

		switch (operands[0].memop.seg) {
		case x86_register_names::FS:
			overridden_opcode |= FE_SEG(FE_FS);
			break;
		case x86_register_names::GS:
			overridden_opcode |= FE_SEG(FE_GS);
			break;
		default:
			break;
		}

		if (!operands[1].is_imm()) {
			throw std::runtime_error("expected imm operand 1");
		}

		rc = fe_enc64(&cur, overridden_opcode, FE_MEM((FeReg)operands[0].memop.pbase, 0, 0, operands[0].memop.displacement), operands[1].immop.u64);
		break;
	}

	case x86_opform::OF_R8_I8:
	case x86_opform::OF_R16_I16:
	case x86_opform::OF_R32_I32:
	case x86_opform::OF_R64_I64:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_imm()) {
			throw std::runtime_error("expected imm operand 1");
		}

		rc = fe_enc64(&cur, raw_opcode, (FeReg)operands[0].pregop.regname, operands[1].immop.u64);
		break;

	default:
		throw std::runtime_error("unsupported operand form");
	}

	if (rc) {
		throw std::runtime_error("encoding failed");
	}

	writer.copy_in(buf, cur - buf);
}

static std::map<unsigned long, std::string> opcode_names = {
	{ FE_MOV8rr, "movb" },
	{ FE_MOV16rr, "movw" },
	{ FE_MOV32rr, "movl" },
	{ FE_MOV32ri, "movl" },
	{ FE_MOV64rr, "movq" },
	{ FE_MOVSXr16m16, "movsx" },
	{ FE_MOVSXr16m32, "movsx" },
	{ FE_MOVSXr16m8, "movsx" },
	{ FE_MOVSXr16r16, "movsx" },
	{ FE_MOVSXr16r32, "movsx" },
	{ FE_MOVSXr16r8, "movsx" },
	{ FE_MOVSXr32m16, "movsx" },
	{ FE_MOVSXr32m32, "movsx" },
	{ FE_MOVSXr32m8, "movsx" },
	{ FE_MOVSXr32r16, "movsx" },
	{ FE_MOVSXr32r32, "movsx" },
	{ FE_MOVSXr32r8, "movsx" },
	{ FE_MOVSXr64m16, "movsx" },
	{ FE_MOVSXr64m32, "movsx" },
	{ FE_MOVSXr64m8, "movsx" },
	{ FE_MOVSXr64r16, "movsx" },
	{ FE_MOVSXr64r32, "movsx" },
	{ FE_MOVSXr64r8, "movsx" },
	{ FE_ADD8rr, "addb" },
	{ FE_ADD16rr, "addw" },
	{ FE_ADD32rr, "addl" },
	{ FE_ADD64rr, "addq" },
	{ FE_XOR8rr, "xorb" },
	{ FE_XOR16rr, "xorw" },
	{ FE_XOR32rr, "xorl" },
	{ FE_XOR64rr, "xorq" },
};

static std::map<x86_register_names, std::string> regnames = {
	{ x86_register_names::AX, "ax" },
	{ x86_register_names::CX, "cx" },
	{ x86_register_names::DX, "dx" },
	{ x86_register_names::BX, "bx" },
	{ x86_register_names::SI, "si" },
	{ x86_register_names::DI, "di" },
	{ x86_register_names::SP, "sp" },
	{ x86_register_names::BP, "bp" },
	{ x86_register_names::R15, "r15" },
};

void x86_instruction::dump(std::ostream &os) const
{
	if (!opcode_names.count(raw_opcode)) {
		os << "#" << std::hex << raw_opcode;
	} else {
		os << opcode_names[raw_opcode];
	}

	for (int i = 0; i < nr_operands; i++) {
		if (operands[i].type != x86_operand_type::invalid) {
			os << " ";
			operands[i].dump(os);
		}
	}

	os << std::endl;
}

void x86_operand::dump(std::ostream &os) const
{
	switch (type) {
	case x86_operand_type::imm:
		os << "$0x" << std::hex << immop.u64;
		break;

	case x86_operand_type::mem:
		os << std::dec << memop.displacement << "(%";

		if (memop.virt_base) {
			os << "V" << std::dec << memop.vbase;
		} else {
			os << regnames[memop.pbase];
		}

		os << ")";
		break;

	case x86_operand_type::preg:
		os << "%" << regnames[pregop.regname] << ":" << std::dec << width;
		break;

	case x86_operand_type::vreg:
		os << "%V" << std::dec << vregop.index << ":" << std::dec << width;
		break;
	}
}
