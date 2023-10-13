#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void shuffle_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
  case XED_ICLASS_PSHUFD: {
	auto slct = ((constant_node *)read_operand(2))->const_val_i();
	auto dst_vec = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), read_operand(0)->val());
	auto src_vec = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), read_operand(1)->val());

    for (int i = 0; i < 4; i++) {
		auto res = builder().insert_vector_extract(src_vec->val(), (int)(slct >> (2 * i) & 0b11));
		dst_vec = builder().insert_vector_insert(dst_vec->val(), i, res->val());
	}

    write_operand(0, dst_vec->val());
    break;
  }

  case XED_ICLASS_SHUFPD: {
    auto dst = read_operand(0);
    auto src1 = auto_cast(value_type::vector(value_type::f64(), 2), read_operand(0));
    auto src2 = auto_cast(value_type::vector(value_type::f64(), 2), read_operand(1));
    auto slct = ((constant_node *)read_operand(2))->const_val_i();

    if (slct & 1) {
      dst = builder().insert_vector_insert(dst->val(), 0, builder().insert_vector_extract(src1->val(), 1)->val());
    } else {
      dst = builder().insert_vector_insert(dst->val(), 0, builder().insert_vector_extract(src1->val(), 0)->val());
    }

    if (slct & 2) {
      dst = builder().insert_vector_insert(dst->val(), 1, builder().insert_vector_extract(src2->val(), 1)->val());
    } else {
      dst = builder().insert_vector_insert(dst->val(), 1, builder().insert_vector_extract(src2->val(), 0)->val());
    }

    write_operand(0, dst->val());
    break;
  }

	default:
		throw std::runtime_error("unsupported shuffle instruction");
	}
}
