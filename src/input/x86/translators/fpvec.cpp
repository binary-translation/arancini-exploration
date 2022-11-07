#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void fpvec_translator::do_translate()
{
	// Right now we're missing the kmovw instruction anyways, but just in case
	if (xed_decoded_inst_masked_vector_operation(xed_inst()))
		throw std::runtime_error("Masked instructions not supported");

	// TODO: do not read dst if we overwrite everything
	auto dest = read_operand(0);
	auto src1 = read_operand(1);
	auto src2 = read_operand(2);

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_VADDSS: {

        src1 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
        src2 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
        auto res = pkt()->insert_add(pkt()->insert_vector_extract(src1->val(), 0)->val(), pkt()->insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, pkt()->insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_VSUBSS: {

        src1 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
        src2 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
        auto res = pkt()->insert_sub(pkt()->insert_vector_extract(src1->val(), 0)->val(), pkt()->insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, pkt()->insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_VDIVSS: {

        src1 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
        src2 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
        auto res = pkt()->insert_div(pkt()->insert_vector_extract(src1->val(), 0)->val(), pkt()->insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, pkt()->insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_MULSS: {

        src1 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
        src2 = pkt()->insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
        auto res = pkt()->insert_mul(pkt()->insert_vector_extract(src1->val(), 0)->val(), pkt()->insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, pkt()->insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	default:
		throw std::runtime_error("unsupported fpvec instruction");
	}
}
