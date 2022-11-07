#include <arancini/input/x86/translators/translators.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/ir/value-type.h>
#include <iostream>
#include <sstream>

using namespace arancini::ir;
using namespace arancini::input::x86;
using namespace arancini::input::x86::translators;

static void initialise_xed()
{
	static bool has_initialised_xed = false;

	if (!has_initialised_xed) {
		xed_tables_init();
		has_initialised_xed = true;
	}
}

/*
This function implements the factory pattern, returning a translator specialized in the
translation for each category of instructions.

TODO: This is quite heavy-weight - need to hold instances of translators ready to go, rather than
instantiating each time.
*/
static std::unique_ptr<translator> get_translator(off_t address, xed_decoded_inst_t *xed_inst)
{
	switch (xed_decoded_inst_get_iclass(xed_inst)) {
	case XED_ICLASS_MOV:
	case XED_ICLASS_LEA:
	case XED_ICLASS_MOVQ:
	case XED_ICLASS_MOVD:
	case XED_ICLASS_MOVSXD:
	case XED_ICLASS_MOVZX:
	case XED_ICLASS_MOVHPS:
	case XED_ICLASS_MOVUPS:
	case XED_ICLASS_MOVAPS:
	case XED_ICLASS_MOVDQA:
	case XED_ICLASS_CQO:
	case XED_ICLASS_CDQE:
		return std::make_unique<mov_translator>();

	case XED_ICLASS_SETNBE:
	case XED_ICLASS_SETNB:
	case XED_ICLASS_SETB:
	case XED_ICLASS_SETBE:
	case XED_ICLASS_SETZ:
	case XED_ICLASS_SETNLE:
	case XED_ICLASS_SETNL:
	case XED_ICLASS_SETL:
	case XED_ICLASS_SETLE:
	case XED_ICLASS_SETNZ:
	case XED_ICLASS_SETNO:
	case XED_ICLASS_SETNP:
	case XED_ICLASS_SETNS:
	case XED_ICLASS_SETO:
	case XED_ICLASS_SETP:
	case XED_ICLASS_SETS:
		return std::make_unique<setcc_translator>();

	case XED_ICLASS_JNBE:
	case XED_ICLASS_JNB:
	case XED_ICLASS_JB:
	case XED_ICLASS_JBE:
	case XED_ICLASS_JZ:
	case XED_ICLASS_JNLE:
	case XED_ICLASS_JNL:
	case XED_ICLASS_JL:
	case XED_ICLASS_JLE:
	case XED_ICLASS_JNZ:
	case XED_ICLASS_JNO:
	case XED_ICLASS_JNP:
	case XED_ICLASS_JNS:
	case XED_ICLASS_JO:
	case XED_ICLASS_JP:
	case XED_ICLASS_JS:
		return std::make_unique<jcc_translator>();

	case XED_ICLASS_CMOVNBE:
	case XED_ICLASS_CMOVNB:
	case XED_ICLASS_CMOVB:
	case XED_ICLASS_CMOVBE:
	case XED_ICLASS_CMOVZ:
	case XED_ICLASS_CMOVNLE:
	case XED_ICLASS_CMOVNL:
	case XED_ICLASS_CMOVL:
	case XED_ICLASS_CMOVLE:
	case XED_ICLASS_CMOVNZ:
	case XED_ICLASS_CMOVNO:
	case XED_ICLASS_CMOVNP:
	case XED_ICLASS_CMOVNS:
	case XED_ICLASS_CMOVO:
	case XED_ICLASS_CMOVP:
	case XED_ICLASS_CMOVS:
		return std::make_unique<cmov_translator>();

	case XED_ICLASS_NOP:
	case XED_ICLASS_HLT:
	case XED_ICLASS_CPUID:
	case XED_ICLASS_SYSCALL:
		return std::make_unique<nop_translator>();

	case XED_ICLASS_XOR:
	case XED_ICLASS_PXOR:
	case XED_ICLASS_AND:
	case XED_ICLASS_PAND:
	case XED_ICLASS_OR:
	case XED_ICLASS_POR:
	case XED_ICLASS_ADD:
	case XED_ICLASS_ADC:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_CMP:
	case XED_ICLASS_TEST:
		return std::make_unique<binop_translator>();

	case XED_ICLASS_PUSH:
	case XED_ICLASS_POP:
	case XED_ICLASS_LEAVE:
		return std::make_unique<stack_translator>();

	case XED_ICLASS_CALL_FAR:
	case XED_ICLASS_CALL_NEAR:
	case XED_ICLASS_RET_FAR:
	case XED_ICLASS_RET_NEAR:
	case XED_ICLASS_JMP:
		return std::make_unique<branch_translator>();

	case XED_ICLASS_SAR:
	case XED_ICLASS_SHR:
	case XED_ICLASS_SHL:
		return std::make_unique<shifts_translator>();

	case XED_ICLASS_NOT:
		return std::make_unique<unop_translator>();

	case XED_ICLASS_IMUL:
	case XED_ICLASS_MUL:
	case XED_ICLASS_IDIV:
		return std::make_unique<muldiv_translator>();

	case XED_ICLASS_REPE_CMPSB:
		return std::make_unique<rep_translator>();

	case XED_ICLASS_PUNPCKLQDQ:
	case XED_ICLASS_PUNPCKLDQ:
		return std::make_unique<punpck_translator>();

	case XED_ICLASS_VADDSS:
	case XED_ICLASS_VSUBSS:
	case XED_ICLASS_VDIVSS:
	case XED_ICLASS_VMULSS:
	case XED_ICLASS_VADDSD:
	case XED_ICLASS_VSUBSD:
	case XED_ICLASS_VDIVSD:
	case XED_ICLASS_VMULSD:
		return std::make_unique<fpvec_translator>();

	default: {
		char buffer[64];
		xed_format_context(XED_SYNTAX_INTEL, xed_inst, buffer, sizeof(buffer), address, nullptr, 0);
		std::cerr << "UNSUPPORTED INSTRUCTION @ " << std::hex << address << ": " << buffer << std::endl;

		throw std::runtime_error("unsupported instruction");
	}
	}
}

/*
This is the starting point for the translation of the input architecture.
For each instruction in the input machine code, it uses a factory pattern
to get a translator specific for that category of instruction, which
is then used to translate the instruction to the Arancini IR.
Each instruction is translated into a packet, which is then added to
the output chunk.

The translator factory is implemented by the get_translator function.
All the x86 translators implementations can be found in the
src/input/x86/translators/ folder.
*/
std::shared_ptr<chunk> x86_input_arch::translate_chunk(off_t base_address, const void *code, size_t code_size, bool basic_block)
{
	auto c = std::make_shared<chunk>();

	initialise_xed();

	const uint8_t *mc = (const uint8_t *)code;

	std::cerr << "chunk @ " << std::hex << base_address << " code=" << code << ", size=" << code_size << std::endl;

	size_t offset = 0;
	do {
		xed_decoded_inst_t xedd;
		xed_decoded_inst_zero(&xedd);
		xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
		xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_ALL);

		xed_error_enum_t xed_error = xed_decode(&xedd, &mc[offset], code_size - offset);
		if (xed_error != XED_ERROR_NONE) {
			throw std::runtime_error("unable to decode instruction: " + std::to_string(xed_error));
		}

		xed_uint_t length = xed_decoded_inst_get_length(&xedd);

		auto t = get_translator(base_address, &xedd);
		auto p = t->translate(base_address, &xedd);
		c->add_packet(p);

		if (p->updates_pc() && basic_block) {
			break;
		}

		offset += length;
		base_address += length;
	} while (offset < code_size);

	return c;
}
