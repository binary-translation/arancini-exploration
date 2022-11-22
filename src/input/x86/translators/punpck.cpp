#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

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
		auto dst = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
		auto src = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op1->val());

		auto src_low = builder().insert_vector_extract(src->val(), 0);
		dst = builder().insert_vector_insert(dst->val(), 1, src_low->val());

		write_operand(0, dst->val());

		break;
	}
	case XED_ICLASS_PUNPCKLDQ: {
        /*
         * op0[31..0] = op0[31..0];
         * op0[63..32] = op1[31..0];
		 * op0[95..64] = op0[63..32];
         * op0[127..96] = op1[63..32];
         */
		op0 = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());

		auto dst = builder().insert_vector_insert(op0->val(), 1, builder().insert_vector_extract(op1->val(), 0)->val());
		dst = builder().insert_vector_insert(dst->val(), 2, builder().insert_vector_extract(op0->val(), 1)->val());
		dst = builder().insert_vector_insert(dst->val(), 3, builder().insert_vector_extract(op1->val(), 1)->val());

		write_operand(0, dst->val());
		break;
	}
	case XED_ICLASS_PUNPCKLWD: {
		/*
		 * op0[15..0]  = op0[15..0]
		 * op0[31..16] = op1[15..0]
		 * op0[47..32] = op0[31..16]
		 * op0[48..63] = op1[31..16]
		 * op0[79..64] = op0[47..32]
		 * op0[95..80] = op1[47..32]
		 * op0[111..96] = op0[63..48]
		 * op0[127..112] = op1[63..48]
		 */
		auto v0 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op0->val());
		auto v1 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op1->val());
		auto dst = v0;
		for (int i = 0; i < 4; i++) {
			if (i)
				dst = builder().insert_vector_insert(dst->val(), 2 * i, builder().insert_vector_extract(v0->val(), i)->val());
			dst = builder().insert_vector_insert(dst->val(), 2 * i + 1, builder().insert_vector_extract(v1->val(), i)->val());
		}
		// auto rslt = builder().insert_bitcast(value_type::u128(), dst->val());

		write_operand(0, dst->val());
		break;
	}
	default:
		throw std::runtime_error("unsupported punpck operation");
	}
}
