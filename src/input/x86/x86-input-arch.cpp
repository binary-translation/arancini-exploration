#include <arancini/input/x86/translators/translators.h>
#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/ir-builder.h>
#include <iostream>
#include <sstream>

using namespace arancini::ir;
using namespace arancini::input;
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
static std::unique_ptr<translator> get_translator(ir_builder &builder, xed_iclass_enum_t ic)
{
	switch (ic) {
	case XED_ICLASS_MOV:
	case XED_ICLASS_LEA:
	case XED_ICLASS_MOVQ:
	case XED_ICLASS_MOVD:
	case XED_ICLASS_MOVSX:
	case XED_ICLASS_MOVSXD:
  case XED_ICLASS_MOVSD:
  case XED_ICLASS_MOVSD_XMM:
  case XED_ICLASS_MOVSQ:
  case XED_ICLASS_MOVZX:
  case XED_ICLASS_MOVHPS:
  case XED_ICLASS_MOVUPS:
  case XED_ICLASS_MOVAPS:
  case XED_ICLASS_MOVDQA:
  case XED_ICLASS_MOVAPD:
  case XED_ICLASS_MOVLPD:
  case XED_ICLASS_MOVHPD:
  case XED_ICLASS_MOVSS:
  case XED_ICLASS_MOVDQU:
	case XED_ICLASS_CQO:
	case XED_ICLASS_CWD:
	case XED_ICLASS_CDQ:
	case XED_ICLASS_CDQE:
		return std::make_unique<mov_translator>(builder);

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
		return std::make_unique<setcc_translator>(builder);

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
		return std::make_unique<jcc_translator>(builder);

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
		return std::make_unique<cmov_translator>(builder);

	case XED_ICLASS_NOP:
	case XED_ICLASS_HLT:
	case XED_ICLASS_CPUID:
	case XED_ICLASS_SYSCALL: // TODO support
	case XED_ICLASS_PREFETCHNTA:
  case XED_ICLASS_UD0: // TODO support
  case XED_ICLASS_UD1: // TODO support
  case XED_ICLASS_UD2: // TODO support
  case XED_ICLASS_FLD: // TODO support
  case XED_ICLASS_FST: // TODO support
  case XED_ICLASS_FSTP: // TODO support
  case XED_ICLASS_FNSTENV: // TODO support
  case XED_ICLASS_FLDENV: // TODO support
  case XED_ICLASS_FWAIT: // TODO support
	  return std::make_unique<nop_translator>(builder);

  case XED_ICLASS_XOR:
  case XED_ICLASS_PXOR:
  case XED_ICLASS_AND:
  case XED_ICLASS_PAND:
  case XED_ICLASS_OR:
  case XED_ICLASS_POR:
  case XED_ICLASS_ADD:
  case XED_ICLASS_ADDSD:
  case XED_ICLASS_ADC:
  case XED_ICLASS_SUB:
  case XED_ICLASS_SBB:
  case XED_ICLASS_CMP:
  case XED_ICLASS_TEST:
  case XED_ICLASS_XADD:
  case XED_ICLASS_BT:
  case XED_ICLASS_BTS:
  case XED_ICLASS_BTR:
  case XED_ICLASS_COMISS:
  // SSE2 binary operations
  case XED_ICLASS_PADDQ:
  case XED_ICLASS_PADDD:
  case XED_ICLASS_PADDW:
  case XED_ICLASS_PADDB:
  case XED_ICLASS_PSUBQ:
  case XED_ICLASS_PSUBD:
  case XED_ICLASS_PSUBW:
  case XED_ICLASS_PSUBB:
		return std::make_unique<binop_translator>(builder);

	case XED_ICLASS_PUSH:
	case XED_ICLASS_POP:
	case XED_ICLASS_LEAVE:
		return std::make_unique<stack_translator>(builder);

	case XED_ICLASS_CALL_FAR:
	case XED_ICLASS_CALL_NEAR:
	case XED_ICLASS_RET_FAR:
	case XED_ICLASS_RET_NEAR:
	case XED_ICLASS_JMP:
		return std::make_unique<branch_translator>(builder);

	case XED_ICLASS_SAR:
	case XED_ICLASS_SHR:
	case XED_ICLASS_SHL:
		return std::make_unique<shifts_translator>(builder);

	case XED_ICLASS_NOT:
	case XED_ICLASS_NEG:
		return std::make_unique<unop_translator>(builder);

	case XED_ICLASS_MUL:
	case XED_ICLASS_IMUL:
	case XED_ICLASS_DIV:
	case XED_ICLASS_IDIV:
	case XED_ICLASS_MULSD:
		return std::make_unique<muldiv_translator>(builder);

	case XED_ICLASS_REPE_CMPSB:
	case XED_ICLASS_REP_STOSQ:
		return std::make_unique<rep_translator>(builder);

	case XED_ICLASS_PUNPCKLQDQ:
	case XED_ICLASS_PUNPCKLDQ:
	case XED_ICLASS_PUNPCKLWD:
	case XED_ICLASS_PUNPCKHWD:
		return std::make_unique<punpck_translator>(builder);

	case XED_ICLASS_VADDSS:
	case XED_ICLASS_VSUBSS:
	case XED_ICLASS_VDIVSS:
	case XED_ICLASS_VMULSS:
	case XED_ICLASS_VADDSD:
	case XED_ICLASS_VSUBSD:
	case XED_ICLASS_VDIVSD:
	case XED_ICLASS_VMULSD:
	case XED_ICLASS_CVTSD2SS:
		return std::make_unique<fpvec_translator>(builder);

	case XED_ICLASS_PSHUFD:
		return std::make_unique<shuffle_translator>(builder);

	case XED_ICLASS_XADD_LOCK:
	case XED_ICLASS_XCHG:
  case XED_ICLASS_CMPXCHG_LOCK:
  case XED_ICLASS_ADD_LOCK:
  case XED_ICLASS_AND_LOCK:
  case XED_ICLASS_OR_LOCK:
  case XED_ICLASS_DEC_LOCK:
		return std::make_unique<atomic_translator>(builder);

  case XED_ICLASS_FNSTCW:
    return std::make_unique<fpu_translator>(builder);

  default:
		return nullptr;
	}
}

static translation_result translate_instruction(ir_builder &builder, size_t address, xed_decoded_inst_t *xedd, bool debug, disassembly_syntax da)
{
	std::string disasm = "";

	if (debug) {
		char buffer[64];
		xed_format_context(da == disassembly_syntax::intel ? XED_SYNTAX_INTEL : XED_SYNTAX_ATT, xedd, buffer, sizeof(buffer) - 1, address, nullptr, 0);
		disasm = std::string(buffer);
	}

	auto t = get_translator(builder, xed_decoded_inst_get_iclass(xedd));

	if (t) {
		return t->translate(address, xedd, disasm);
	} else {
    std::cerr << "Could not find a translator for: " << disasm << std::endl;
		return translation_result::fail;
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
void x86_input_arch::translate_chunk(ir_builder &builder, off_t base_address, const void *code, size_t code_size, bool basic_block)
{
	builder.begin_chunk();

	initialise_xed();

	const uint8_t *mc = (const uint8_t *)code;

	std::cerr << "chunk @ " << std::hex << base_address << " code=" << code << ", size=" << code_size << std::endl;

	size_t offset = 0;
	while (offset < code_size) {
		xed_decoded_inst_t xedd;
		xed_decoded_inst_zero(&xedd);
		xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
		xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_ALL);

		xed_error_enum_t xed_error = xed_decode(&xedd, &mc[offset], code_size - offset);
		if (xed_error != XED_ERROR_NONE) {
			throw std::runtime_error("unable to decode instruction: " + std::to_string(xed_error));
		}

		xed_uint_t length = xed_decoded_inst_get_length(&xedd);

		auto r = translate_instruction(builder, base_address, &xedd, debug(), da_);

		if (r == translation_result::fail) {
			throw std::runtime_error("instruction translation failure: " + std::to_string(xed_error));
		} else if (r == translation_result::end_of_block && basic_block) {
			break;
		}

		offset += length;
		base_address += length;
	}

	builder.end_chunk();
}
