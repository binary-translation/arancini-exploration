#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = auto_cast(op0->val().type(), read_operand(1));

	value_node *rslt;

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
		rslt = pkt()->insert_xor(op0->val(), op1->val());
		break;
	case XED_ICLASS_AND:
	case XED_ICLASS_TEST:
		rslt = pkt()->insert_and(op0->val(), op1->val());
		break;
	case XED_ICLASS_OR:
		rslt = pkt()->insert_or(op0->val(), op1->val());
		break;

	case XED_ICLASS_ADD:
		rslt = pkt()->insert_add(op0->val(), op1->val());
		break;
	case XED_ICLASS_ADC:
		rslt = pkt()->insert_adc(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;
	case XED_ICLASS_SUB:
	case XED_ICLASS_CMP:
		rslt = pkt()->insert_sub(op0->val(), op1->val());
		break;
	case XED_ICLASS_SBB:
		rslt = pkt()->insert_sbb(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;

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
