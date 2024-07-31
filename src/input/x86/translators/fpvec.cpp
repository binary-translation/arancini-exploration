#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

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
	case XED_ICLASS_XORPS: {
		dest = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), dest->val());
		src1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
		src2 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
	} break;
	case XED_ICLASS_VADDSS:
	case XED_ICLASS_VSUBSS:
	case XED_ICLASS_VDIVSS:
	case XED_ICLASS_VMULSS: {
		dest = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), dest->val());
		src1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
    src2 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), src2->val());
		break;
	}
	case XED_ICLASS_XORPD:
	case XED_ICLASS_VADDSD:
	case XED_ICLASS_VSUBSD:
	case XED_ICLASS_VDIVSD:
	case XED_ICLASS_VMULSD: {
		dest = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), dest->val());
		src1 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), src1->val());
    src2 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), src2->val());
		break;
	}
	case XED_ICLASS_CVTSD2SS:
	case XED_ICLASS_CVTSD2SI:
	case XED_ICLASS_CVTTSD2SI: {
		if (src1->val().type().element_width() == 128) {
			src1 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), src1->val());
			src1 = builder().insert_vector_extract(src1->val(), 0);
		} else {
			src1 = builder().insert_bitcast(value_type::f64(), src1->val());
		}
		break;
	}

  case XED_ICLASS_CVTSI2SS: {
    src1 = builder().insert_convert(value_type::f32(), src1->val(), fp_convert_type::round);
    dest = builder().insert_bit_insert(dest->val(), src1->val(), 0, 32);
    write_operand(0, dest->val());
    break;
  }

  case XED_ICLASS_CVTSI2SD: {
    src1 = builder().insert_convert(value_type::f64(), src1->val(), fp_convert_type::round);
    dest = builder().insert_bit_insert(dest->val(), src1->val(), 0, 64);
    write_operand(0, dest->val());
    break;
  }
  case XED_ICLASS_CVTSS2SD:
  case XED_ICLASS_CVTSS2SI:
  case XED_ICLASS_CVTTSS2SI: {
	if (src1->val().type().element_width() == 128) {
		src1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), src1->val());
		src1 = builder().insert_vector_extract(src1->val(), 0);
	} else {
		src1 = builder().insert_bitcast(value_type::f32(), src1->val());
	}
	break;
  }

	default:
		throw std::runtime_error("Unknown fpvec instruction");
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XORPD:
	case XED_ICLASS_XORPS: {
		auto res = builder().insert_xor(src1->val(), src2->val());
		write_operand(0, res->val());
	} break;
	case XED_ICLASS_VADDSS:
	case XED_ICLASS_VADDSD: {

        auto res = builder().insert_add(builder().insert_vector_extract(src1->val(), 0)->val(), builder().insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_VSUBSS:
	case XED_ICLASS_VSUBSD: {

        auto res = builder().insert_sub(builder().insert_vector_extract(src1->val(), 0)->val(), builder().insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_VDIVSS:
	case XED_ICLASS_VDIVSD: {

        auto res = builder().insert_div(builder().insert_vector_extract(src1->val(), 0)->val(), builder().insert_vector_extract(src2->val(), 0)->val());

        write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_VMULSS:
	case XED_ICLASS_VMULSD: {
        	auto res = builder().insert_mul(builder().insert_vector_extract(src1->val(), 0)->val(), builder().insert_vector_extract(src2->val(), 0)->val());

        	write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_MULSS:
	case XED_ICLASS_MULSD: {
        	auto res = builder().insert_mul(builder().insert_vector_extract(src1->val(), 0)->val(), builder().insert_vector_extract(src2->val(), 0)->val());

        	write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
		break;
	}
	case XED_ICLASS_CVTSD2SI:
	case XED_ICLASS_CVTSD2SS: {
		auto res = builder().insert_convert(value_type::f32(), src1->val(), fp_convert_type::round);
		dest = builder().insert_vector_insert(dest->val(), 0, res->val());

		write_operand(0, dest->val());
		break;
	}
	case XED_ICLASS_CVTTSD2SI:
	case XED_ICLASS_CVTTSS2SI: {
		dest = builder().insert_convert(dest->val().type(), src1->val(), fp_convert_type::trunc);
		write_operand(0, dest->val());
		break;
	}
	case XED_ICLASS_CVTSS2SI:
	case XED_ICLASS_CVTSS2SD: {
		dest = builder().insert_convert(dest->val().type(), src1->val(), fp_convert_type::none);
		write_operand(0, dest->val());
		break;
	}
	default:
    		break;
	}
}
