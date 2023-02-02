#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void shifts_translator::do_translate()
{
	auto src = read_operand(0);
	auto amt = builder().insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst()));

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_SAR:
		write_operand(0, builder().insert_asr(src->val(), amt->val())->val());
		break;

	case XED_ICLASS_SHR:
		write_operand(0, builder().insert_lsr(src->val(), amt->val())->val());
		break;

	case XED_ICLASS_SHL:
		write_operand(0, builder().insert_lsl(src->val(), amt->val())->val());
		break;

	case XED_ICLASS_PSLLDQ: {
    auto amt_val = ((constant_node *)amt)->const_val_i();

    if (amt_val > 15) {
      write_operand(0, builder().insert_constant_u128(0)->val());
    } else {
      write_operand(0, builder().insert_lsl(src->val(), builder().insert_constant_u32(amt_val * 8)->val())->val());
    }
    break;
	}

  case XED_ICLASS_PSRLDQ: {
    auto amt_val = ((constant_node *)amt)->const_val_i();

    if (amt_val > 15) {
      write_operand(0, builder().insert_constant_u128(0)->val());
    } else {
      write_operand(0, builder().insert_lsr(src->val(), builder().insert_constant_u32(amt_val * 8)->val())->val());
    }
    break;
  }

  case XED_ICLASS_ROR: {
    // xed encoding: ror reg, imm
    auto value = read_operand(0);
    constant_node *rot = (constant_node *)read_operand(1); // TODO might need to mask this to 5 or 6 bits
    auto rot_val = rot->const_val_i();

    if (rot_val != 0) {
      auto low_bits = builder().insert_bit_extract(value->val(), 0, rot_val);
      auto shift = builder().insert_lsr(value->val(), rot->val());
      auto res = builder().insert_bit_insert(shift->val(), low_bits->val(), value->val().type().width() - rot_val, rot_val);

      write_operand(0, res->val());

      // CF flag receives a copy of the last bit that was shifted from one end to the other
      auto top_bit0 = builder().insert_bit_extract(res->val(), res->val().type().width() - 1, 1);
      write_reg(reg_offsets::CF, top_bit0->val());

      if (rot_val == 1) {
        // OF is set to the XOR of 2 most significant bits iff rot == 1
        auto top_bit1 = builder().insert_bit_extract(res->val(), res->val().type().width() - 2, 1);
        write_reg(reg_offsets::OF, builder().insert_xor(top_bit0->val(), top_bit1->val())->val());
      }
    }
    break;
  }

  case XED_ICLASS_ROL: {
    // xed encoding: rol reg, imm
    auto value = read_operand(0);
    constant_node *rot = (constant_node *)read_operand(1); // TODO might need to mask this to 5 or 6 bits
    auto rot_val = rot->const_val_i();

    if (rot_val != 0) {
      auto high_bits = builder().insert_bit_extract(value->val(), value->val().type().width() - rot_val, rot_val);
      auto shift = builder().insert_lsl(value->val(), rot->val());
      auto res = builder().insert_bit_insert(shift->val(), high_bits->val(), 0, rot_val);

      write_operand(0, res->val());

      // CF flag receives a copy of the last bit that was shifted from one end to the other
      auto low_bit = builder().insert_bit_extract(res->val(), 0, 1);
      write_reg(reg_offsets::CF, low_bit->val());

      if (rot_val == 1) {
        // OF is set to the XOR of CF (after rotate) and the most significant bits iff rot == 1
        auto top_bit = builder().insert_bit_extract(res->val(), res->val().type().width() - 1, 1);
        write_reg(reg_offsets::OF, builder().insert_xor(top_bit->val(), low_bit->val())->val());
      }
    }
    break;
  }

	default:
		throw std::runtime_error("unsupported shift instruction");
	}
}
