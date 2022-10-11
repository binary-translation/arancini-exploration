#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void punpck_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = read_operand(1);

	switch (xed_decoded_inst_get_iclass(xed_inst())) {

	case XED_ICLASS_PUNPCKLQDQ: {
        /*
         * op0[63..0] = op0[63..0];
         * op0[127..64] = op1[63..0];
         */
		auto dst = pkt()->insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
		auto src = pkt()->insert_bitcast(value_type::vector(value_type::u64(), 2), op1->val());

		write_operand(0, pkt()->insert_vector_insert(dst->val(), 1, pkt()->insert_vector_extract(src->val(), 0)->val())->val());

		break;
	}
	case XED_ICLASS_PUNPCKLDQ: {
        /*
         * op0[31..0] = op0[31..0];
         * op0[63..32] = op1[63..32];
		 * op0[95..64] = op0[95..64];
         * op0[127..96] = op1[127..96];
         */
		auto dst = pkt()->insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
		auto src = pkt()->insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());

		auto ex_lo = pkt()->insert_vector_extract(src->val(), 1);
		auto ex_hi = pkt()->insert_vector_extract(src->val(), 3);

		write_operand(0, pkt()->insert_vector_insert(pkt()->insert_vector_insert(dst->val(), 1, ex_lo->val())->val(), 3, ex_hi->val())->val());
		break;
	}
	default:
		throw std::runtime_error("unsupported punpck operation");
	}
}
