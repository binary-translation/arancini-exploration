#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void mov_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_LEA:
		write_operand(0, compute_address(0)->val());
		break;

	case XED_ICLASS_MOV: {
		const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
		auto operand = xed_inst_operand(insn, 0);
		auto opname = xed_operand_name(operand);

		auto tt = opname == XED_OPERAND_MEM0 ? type_of_operand(1) : type_of_operand(0);
		auto op1 = auto_cast(tt, read_operand(1));
		write_operand(0, op1->val());
		break;
	}

	case XED_ICLASS_CQO: {
		// TODO: Operand sizes
		auto sign_set = read_reg(value_type::u64(), reg_offsets::rax);
		sign_set = builder().insert_trunc(value_type::u1(), builder().insert_asr(sign_set->val(), builder().insert_constant_u32(63)->val())->val());

		auto sx = builder().insert_csel(sign_set->val(), builder().insert_constant_u64(0xffffffffffffffffull)->val(), builder().insert_constant_u64(0)->val());
		write_reg(reg_offsets::rdx, sx->val());
		break;
	}

	case XED_ICLASS_CDQE: {
		/* Only for 64-bit, other sizes are with CBW/CWDE instructions */
		auto eax = read_reg(value_type::s32(), reg_to_offset(XED_REG_EAX));
		auto rax = builder().insert_sx(value_type::s64(), eax->val());
		write_reg(reg_to_offset(XED_REG_RAX), rax->val());
		break;
	}

	case XED_ICLASS_MOVQ: {
		// TODO: INCORRECT FOR SOME SIZES
		auto op1 = read_operand(1);
		write_operand(0, op1->val());
		break;
	}

	case XED_ICLASS_MOVSXD: {
		// TODO: INCORRECT FOR SOME SIZES
		auto input = read_operand(1);
		input = builder().insert_bitcast(value_type(value_type_class::signed_integer, input->val().type().width()), input->val());

		auto cast = builder().insert_sx(value_type::s64(), input->val());

		write_operand(0, cast->val());
		break;
	}

	case XED_ICLASS_MOVZX: {
		// TODO: Incorrect operand sizes
		auto input = read_operand(1);
		auto cast = builder().insert_zx(value_type::u64(), input->val());

		write_operand(0, cast->val());
		break;
	}

	case XED_ICLASS_MOVD: {
		auto input = read_operand(1);
		auto cast = builder().insert_zx(value_type::u32(), input->val());

		write_operand(0, cast->val());
		break;
	}

	case XED_ICLASS_MOVHPS:
	case XED_ICLASS_MOVUPS:
	case XED_ICLASS_MOVAPS:
	case XED_ICLASS_MOVDQA: {
		// TODO: INCORRECT FOR SOME SIZES

		auto src = read_operand(1);
		// auto dst = read_operand(pkt, xed_inst, 0);
		// auto result = pkt->insert_or(dst->val(), )
		write_operand(0, src->val());
		break;
	}

	default:
		throw std::runtime_error("unsupported mov operation");
	}
}
