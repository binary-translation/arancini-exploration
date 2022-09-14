#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void muldiv_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = read_operand(1);

	switch (xed_decoded_inst_get_iclass(xed_inst())) {

	case XED_ICLASS_IMUL: {
		auto rslt = pkt()->insert_mul(op0->val(), op1->val());

		write_operand(0, rslt->val());
		write_flags(rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	case XED_ICLASS_IDIV: {
		auto rslt = pkt()->insert_div(op0->val(), op1->val());

		write_operand(0, rslt->val());
		write_flags(rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	default:
		throw std::runtime_error("unsupported mul/div operation");
	}
}
