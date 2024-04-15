#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void punpck_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = read_operand(1);

	auto inst = xed_decoded_inst_get_iclass(xed_inst());
  switch (inst) {
	// case XED_ICLASS_PUNPCKLQDQ: {
  //   /*
  //    * op0[63..0] = op0[63..0];
  //    * op0[127..64] = op1[63..0];
  //    */
	// 	auto dst = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
	// 	auto src = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op1->val());

	// 	auto src_low = builder().insert_vector_extract(src->val(), 0);
	// 	dst = builder().insert_vector_insert(dst->val(), 1, src_low->val());

	// 	write_operand(0, dst->val());

	// 	break;
	// }
	// case XED_ICLASS_PUNPCKLDQ: {
  //   /*
  //    * op0[31..0] = op0[31..0];
  //    * op0[63..32] = op1[31..0];
  //    * op0[95..64] = op0[63..32];
  //    * op0[127..96] = op1[63..32];
  //    */
	// 	op0 = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
	// 	op1 = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());

	// 	auto dst = builder().insert_vector_insert(op0->val(), 1, builder().insert_vector_extract(op1->val(), 0)->val());
	// 	dst = builder().insert_vector_insert(dst->val(), 2, builder().insert_vector_extract(op0->val(), 1)->val());
	// 	dst = builder().insert_vector_insert(dst->val(), 3, builder().insert_vector_extract(op1->val(), 1)->val());

	// 	write_operand(0, dst->val());
	// 	break;
	// }
	// case XED_ICLASS_PUNPCKLWD: {
	// 	/*
	// 	 * op0[15..0]  = op0[15..0]
	// 	 * op0[31..16] = op1[15..0]
	// 	 * op0[47..32] = op0[31..16]
	// 	 * op0[48..63] = op1[31..16]
	// 	 * op0[79..64] = op0[47..32]
	// 	 * op0[95..80] = op1[47..32]
	// 	 * op0[111..96] = op0[63..48]
	// 	 * op0[127..112] = op1[63..48]
	// 	 */
	// 	auto v0 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op0->val());
	// 	auto v1 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op1->val());
	// 	auto dst = v0;
	// 	for (int i = 0; i < 4; i++) {
	// 		if (i)
	// 			dst = builder().insert_vector_insert(dst->val(), 2 * i, builder().insert_vector_extract(v0->val(), i)->val());
	// 		dst = builder().insert_vector_insert(dst->val(), 2 * i + 1, builder().insert_vector_extract(v1->val(), i)->val());
	// 	}

	// 	write_operand(0, dst->val());
	// 	break;
	// }

  case XED_ICLASS_PUNPCKLBW:
  case XED_ICLASS_PUNPCKLWD:
  case XED_ICLASS_PUNPCKLDQ:
  case XED_ICLASS_PUNPCKLQDQ: {
    auto input_size = op0->val().type().width();
    auto elt_size = (inst == XED_ICLASS_PUNPCKLBW) ? 8 : (inst == XED_ICLASS_PUNPCKLWD) ? 16 : (inst == XED_ICLASS_PUNPCKLDQ) ? 32 : 64;
    auto elt_type = value_type(value_type_class::unsigned_integer, elt_size);
		auto v0 = builder().insert_bitcast(value_type::vector(elt_type, input_size / elt_size), op0->val());
		auto v1 = builder().insert_bitcast(value_type::vector(elt_type, input_size / elt_size), op1->val());
		auto dst = v0;

    for (int i = 0; i < input_size / elt_size / 2; i++) {
			dst = builder().insert_vector_insert(dst->val(), i * 2, builder().insert_vector_extract(v0->val(), i)->val());
			dst = builder().insert_vector_insert(dst->val(), i * 2 + 1, builder().insert_vector_extract(v1->val(), i)->val());
    }

    write_operand(0, dst->val());
    break;
  }

	// case XED_ICLASS_PUNPCKHWD: {
	// 	/*
	// 	 * Destination[0..15] = Destination[64..79];
	// 	 * Destination[16..31] = Source[64..79];
	// 	 * Destination[32..47] = Destination[80..95];
	// 	 * Destination[48..63] = Source[80..95];
	// 	 * Destination[64..79] = Destination[96..111];
	// 	 * Destination[80..95] = Source[96..111];
	// 	 * Destination[96..111] = Destination[112..127];
	// 	 * Destination[112..127] = Source[112..127];
	// 	 */
	// 	auto v0 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op0->val());
	// 	auto v1 = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op1->val());
	// 	auto dst = v0;
	// 	for (int i = 0; i < 4; i++) {
	// 		dst = builder().insert_vector_insert(dst->val(), 2 * i, builder().insert_vector_extract(v0->val(), i + 4)->val());
	// 		dst = builder().insert_vector_insert(dst->val(), 2 * i + 1, builder().insert_vector_extract(v1->val(), i + 4)->val());
	// 	}

	// 	write_operand(0, dst->val());
	// 	break;
	// }

  case XED_ICLASS_PUNPCKHBW:
	case XED_ICLASS_PUNPCKHWD:
  case XED_ICLASS_PUNPCKHDQ:
  case XED_ICLASS_PUNPCKHQDQ: {
    auto input_size = op0->val().type().width();
    auto elt_size = (inst == XED_ICLASS_PUNPCKHBW) ? 8 : (inst == XED_ICLASS_PUNPCKHWD) ? 16 : (inst == XED_ICLASS_PUNPCKHDQ) ? 32 : 64;
    auto elt_type = value_type(value_type_class::unsigned_integer, elt_size);
		auto v0 = builder().insert_bitcast(value_type::vector(elt_type, input_size / elt_size), op0->val());
		auto v1 = builder().insert_bitcast(value_type::vector(elt_type, input_size / elt_size), op1->val());
		auto dst = v0;

    for (int i = 0; i < input_size / elt_size / 2; i++) {
      dst = builder().insert_vector_insert(dst->val(), 2 * i,
                                           builder().insert_vector_extract(v0->val(), i + v0->val().type().nr_elements() / 2)->val());
      dst = builder().insert_vector_insert(dst->val(), 2 * i + 1,
                                           builder().insert_vector_extract(v1->val(), i + v1->val().type().nr_elements() / 2)->val());
    }

    write_operand(0, dst->val());
    break;
  }

  case XED_ICLASS_PACKUSWB: {
		/*
		 * DEST[7:0] := SaturateSignedWordToUnsignedByte DEST[15:0];
     * DEST[15:8] := SaturateSignedWordToUnsignedByte DEST[31:16];
     * DEST[23:16] := SaturateSignedWordToUnsignedByte DEST[47:32];
     * DEST[31:24] := SaturateSignedWordToUnsignedByte DEST[63:48];
     * DEST[39:32] := SaturateSignedWordToUnsignedByte SRC[15:0];
     * DEST[47:40] := SaturateSignedWordToUnsignedByte SRC[31:16];
     * DEST[55:48] := SaturateSignedWordToUnsignedByte SRC[47:32];
     * DEST[63:56] := SaturateSignedWordToUnsignedByte SRC[63:48];
     * (until 128 bits for xmm inputs)
     * Saturate: if signed word > unsigned byte, make it 0xFF, if negative, make it 0x00
     */
    auto nr_splits = (op0->val().type().width() == 64) ? 4 : 8;
    auto dst = builder().insert_bitcast(value_type::vector(value_type::s16(), nr_splits), op0->val());
    auto src = builder().insert_bitcast(value_type::vector(value_type::s16(), nr_splits), op1->val());
    auto dst_bytes = builder().insert_bitcast(value_type::vector(value_type::s8(), nr_splits * 2), op0->val());

    for (int i = 0; i < nr_splits * 2; i++) {
      auto word = (i < nr_splits) ? builder().insert_vector_extract(dst->val(), i) : builder().insert_vector_extract(src->val(), i - nr_splits);
      auto neg_test = builder().insert_cmpgt(builder().insert_constant_s16(0)->val(), word->val());
      cond_br_node *br_neg = (cond_br_node *)builder().insert_cond_br(neg_test->val(), nullptr);
      auto overflow_test = builder().insert_cmpne(builder().insert_and(word->val(), builder().insert_constant_s16(0xFF00)->val())->val(), builder().insert_constant_s16(0)->val());
      cond_br_node *br_of = (cond_br_node *)builder().insert_cond_br(overflow_test->val(), nullptr);

      // word fits in u8, set dst to truncated word
      auto unsign = builder().insert_bitcast(value_type::u16(), word->val());
      dst_bytes = builder().insert_vector_insert(dst_bytes->val(), i, builder().insert_trunc(value_type::u8(), unsign->val())->val());
      br_node *br_end = (br_node *)builder().insert_br(nullptr);

      // word is negative, set dst to 0x00
      auto neg_label = builder().insert_label("negative");
      br_neg->add_br_target(neg_label);
      dst_bytes = builder().insert_vector_insert(dst_bytes->val(), i, builder().insert_constant_u8(0)->val());
      br_node *br_end2 = (br_node *)builder().insert_br(nullptr);

      // word overflows for 8-bit unsigned, set dst to 0xFF
      auto of_label = builder().insert_label("overflow");
      br_of->add_br_target(of_label);
      dst_bytes = builder().insert_vector_insert(dst_bytes->val(), i, builder().insert_constant_u8(0xFF)->val());

      auto end_label = builder().insert_label("end");
      auto end_label1 = builder().insert_label("end");
      br_end->add_br_target(end_label);
      br_end2->add_br_target(end_label1);
    }
    write_operand(0, dst_bytes->val());
    break;
  }
  case XED_ICLASS_PACKSSDW:
  case XED_ICLASS_PACKSSWB: {
    auto bits = op0->val().type().width();
	auto pre_ty = value_type::v();
	auto pst_ty = value_type::v();
	value_node *cmp_max;
	value_node *cmp_min;
	value_node *ins_max;
	value_node *ins_min;
	switch(inst) {
		case XED_ICLASS_PACKSSDW: {
			pre_ty = value_type::s32();
			pst_ty = value_type::s16();
			cmp_max = builder().insert_constant_s32(0x00007FFF);
			cmp_min = builder().insert_constant_s32(0xFFFF8000);
			ins_max = builder().insert_constant_s16(0x7FFF);
			ins_min = builder().insert_constant_s16(0x8000);
		} break;
		case XED_ICLASS_PACKSSWB: {
			pre_ty = value_type::s16();
			pst_ty = value_type::s8();
			cmp_max = builder().insert_constant_s16(0x007F);
			cmp_min = builder().insert_constant_s16(0xFF80);
			ins_max = builder().insert_constant_s8(0x7F);
			ins_min = builder().insert_constant_s8(0x80);
		} break;
	}
    auto dst = builder().insert_bitcast(value_type::vector(pre_ty, bits/pre_ty.width()), op0->val());
    auto src = builder().insert_bitcast(value_type::vector(pre_ty, bits/pre_ty.width()), op1->val());
    auto result = builder().insert_bitcast(value_type::vector(pst_ty, bits/pst_ty.width()), op0->val());

    for (int i = 0; i < bits/pst_ty.width(); i++) {
		// word/double word
		auto word = (i < bits/pre_ty.width()) ? builder().insert_vector_extract(dst->val(), i) : builder().insert_vector_extract(src->val(), i - bits/pre_ty.width());
      	auto gt_max = builder().insert_cmpgt(word->val(), cmp_max->val());
      	cond_br_node *br_max = (cond_br_node *)builder().insert_cond_br(gt_max->val(), nullptr);
      	auto lt_min = builder().insert_cmpgt(cmp_min->val(), word->val());
      	cond_br_node *br_min = (cond_br_node *)builder().insert_cond_br(lt_min->val(), nullptr);

		// element fits
      	result = builder().insert_vector_insert(result->val(), i, builder().insert_trunc(pst_ty, word->val())->val());
      	br_node *br_end = (br_node *)builder().insert_br(nullptr);

      	// set element to min
      	auto min_label = builder().insert_label("lt_min");
      	br_min->add_br_target(min_label);
      	result = builder().insert_vector_insert(result->val(), i, ins_min->val());
      	br_node *br_end2 = (br_node *)builder().insert_br(nullptr);

      	// set element tot max
      	auto max_label = builder().insert_label("gt_max");
      	br_max->add_br_target(max_label);
      	result = builder().insert_vector_insert(result->val(), i, ins_max->val());

      	auto end_label = builder().insert_label("end");
      	auto end_label1 = builder().insert_label("end");
      	br_end->add_br_target(end_label);
      	br_end2->add_br_target(end_label1);
    }
    write_operand(0, result->val());
    break;
  }

	default:
		throw std::runtime_error("unsupported punpck operation");
	}
}
