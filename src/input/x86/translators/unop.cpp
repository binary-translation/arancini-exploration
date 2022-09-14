#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void unop_translator::do_translate()
{
	auto op0 = read_operand(0);

	value_node *rslt;

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_NOT:
		rslt = pkt()->insert_not(op0->val());
		break;

	default:
		throw std::runtime_error("unsupported unop");
	}

	write_operand(0, rslt->val());

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_NOT:
		write_flags(rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;

	default:
		break;
	}
}
