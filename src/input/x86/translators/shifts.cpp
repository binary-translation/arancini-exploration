#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void shifts_translator::do_translate()
{
	auto src = read_operand(0);
	auto amt = pkt()->insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst()));

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_SAR:
		write_operand(0, pkt()->insert_asr(src->val(), amt->val())->val());
		break;

	case XED_ICLASS_SHR:
		write_operand(0, pkt()->insert_lsr(src->val(), amt->val())->val());
		break;

	case XED_ICLASS_SHL:
		write_operand(0, pkt()->insert_lsl(src->val(), amt->val())->val());
		break;

	default:
		throw std::runtime_error("unsupported shift instruction");
	}
}
