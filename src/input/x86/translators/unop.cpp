#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void unop_translator::do_translate()
{
	auto op0 = read_operand(0);

	value_node *rslt;

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_NOT:
		rslt = builder().insert_not(op0->val());
		break;

	case XED_ICLASS_NEG: {
		// neg writes the negation of the value in the same register
		// also sets CF to 1 if the value is not 0, and to 0 if value is 0
		// Here, we assume that the comparison's value is 1 if true, 0 if false
		auto zero = builder().insert_constant_i(op0->val().type(), 0);
		auto cmpz = builder().insert_cmpne(op0->val(), zero->val());
		cmpz = builder().insert_trunc(value_type::u1(), cmpz->val());
		rslt = builder().insert_sub(zero->val(), op0->val());
		write_reg(reg_offsets::CF, cmpz->val());
		break;
	}

	default:
		throw std::runtime_error("unsupported unop");
	}

	write_operand(0, rslt->val());

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_NOT:
		write_flags(rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;
	case XED_ICLASS_NEG:
		write_flags(rslt, flag_op::update, flag_op::ignore, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
		break;

	default:
		break;
	}
}
