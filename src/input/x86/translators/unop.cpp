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

	case XED_ICLASS_INC:
		rslt = builder().insert_add(op0->val(), builder().insert_constant_i(op0->val().type(), 1)->val());
		break;

	case XED_ICLASS_DEC:
		rslt = builder().insert_sub(op0->val(), builder().insert_constant_i(op0->val().type(), 1)->val());
		break;

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

  case XED_ICLASS_BSWAP: {
    auto src = read_operand(0);
    auto nr_bytes = src->val().type().width() == 32 ? 4 : 8;

    src = builder().insert_bitcast(value_type::vector(value_type::u8(), nr_bytes), src->val());
    rslt = src;

    for (int i = 0; i < nr_bytes; i++) {
      rslt = builder().insert_vector_insert(rslt->val(), i, builder().insert_vector_extract(src->val(), nr_bytes - i - 1)->val());
    }

    break;
  }

  case XED_ICLASS_PMOVMSKB: {
    auto src = read_operand(1);
    auto nr_bytes = src->val().type().width() == 64 ? 8 : 16;

    if (nr_bytes == 8) {
      rslt = builder().insert_constant_i(value_type::u8(), 0);
    } else {
      rslt = builder().insert_constant_i(value_type::u16(), 0);
    }

    for (int i = 0; i < nr_bytes; i++) {
      auto top_bit = builder().insert_bit_extract(src->val(), (i + 1) * 8 - 1, 1);
      rslt = builder().insert_bit_insert(rslt->val(), top_bit->val(), i, 1);
    }

    rslt = builder().insert_zx(value_type(value_type_class::unsigned_integer, get_operand_width(0)), rslt->val());
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
  case XED_ICLASS_INC:
  case XED_ICLASS_DEC:
  case XED_ICLASS_NEG:
		write_flags(rslt, flag_op::update, flag_op::ignore, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
		break;

	default:
		break;
	}
}
