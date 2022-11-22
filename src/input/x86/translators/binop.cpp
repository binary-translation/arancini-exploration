#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = auto_cast(op0->val().type(), read_operand(1));

	value_node *rslt;

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_PXOR:
		rslt = builder().insert_xor(op0->val(), op1->val());
		break;
	case XED_ICLASS_AND:
	case XED_ICLASS_PAND:
	case XED_ICLASS_TEST:
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	case XED_ICLASS_OR:
	case XED_ICLASS_POR:
		rslt = builder().insert_or(op0->val(), op1->val());
		break;

	case XED_ICLASS_ADD:
		rslt = builder().insert_add(op0->val(), op1->val());
		break;
	case XED_ICLASS_ADC:
		rslt = builder().insert_adc(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;
	case XED_ICLASS_SUB:
	case XED_ICLASS_CMP:
		rslt = builder().insert_sub(op0->val(), op1->val());
		break;
	case XED_ICLASS_SBB:
		rslt = builder().insert_sbb(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;
	case XED_ICLASS_PADDD: {
		// only the SSE2 version of the instruction with xmm registers is supported, not the "normal" one with GPRs
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());
		rslt = builder().insert_add(lhs->val(), rhs->val());
		break;
	}
	default:
		throw std::runtime_error("unsupported binop");
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_CMP:
	case XED_ICLASS_TEST:
		break;

	default:
		write_operand(0, rslt->val());
		break;
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_AND:
	case XED_ICLASS_TEST:
	case XED_ICLASS_OR:
		write_flags(rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;

	case XED_ICLASS_ADD:
	case XED_ICLASS_ADC:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_CMP:
		write_flags(rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
		break;

	default:
		break;
	}
}
