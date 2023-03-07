#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = auto_cast(op0->val().type(), read_operand(1));

	value_node *rslt;

  auto inst_class = xed_decoded_inst_get_iclass(xed_inst());
	switch (inst_class) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_PXOR:
		rslt = builder().insert_xor(op0->val(), op1->val());
		break;
	case XED_ICLASS_AND:
	case XED_ICLASS_PAND:
	case XED_ICLASS_TEST:
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	case XED_ICLASS_OR:
	case XED_ICLASS_POR:
		rslt = builder().insert_or(op0->val(), op1->val());
		break;

	case XED_ICLASS_ADD:
		rslt = builder().insert_add(op0->val(), op1->val());
		break;
  case XED_ICLASS_ADDSD: {
    auto dst = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
    auto src = read_operand(1);

    if (src->val().type().width() == 64) { // addsd xmm1, m64
      rslt = builder().insert_add(builder().insert_vector_extract(dst->val(), 0)->val(), src->val());
    } else { // addsd xmm1, xmm2
      src = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), src->val());
      rslt = builder().insert_add(builder().insert_vector_extract(dst->val(), 0)->val(), builder().insert_vector_extract(src->val(), 0)->val());
    }
    dst = builder().insert_vector_insert(dst->val(), 0, rslt->val());
    write_operand(0, dst->val());
    break;
  }
	case XED_ICLASS_ADC:
		rslt = builder().insert_adc(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::CF))->val());
		break;
	case XED_ICLASS_SUB:
	case XED_ICLASS_CMP:
		rslt = builder().insert_sub(op0->val(), op1->val());
		break;
	case XED_ICLASS_SBB:
		rslt = builder().insert_sbb(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::CF))->val());
		break;

  case XED_ICLASS_SUBSS:
	case XED_ICLASS_SUBSD: {
    auto size = (inst_class == XED_ICLASS_SUBSS) ? 32 : 64;

    auto op0_low = builder().insert_bitcast(value_type(value_type_class::floating_point, size),
                                            builder().insert_bit_extract(op0->val(), 0, size)->val());
    if (op1->val().type().width() == 128) {
      op1 = builder().insert_bit_extract(op1->val(), 0, size);
    }
    op1 = builder().insert_bitcast(value_type(value_type_class::floating_point, size), op1->val());

    auto sub = builder().insert_sub(op0_low->val(), op1->val());
    rslt = builder().insert_bit_insert(op0->val(), sub->val(), 0, size);
    break;
  }
	// only the SSE2 version of the instruction with xmm registers is supported, not the "normal" one with GPRs
	case XED_ICLASS_PADDQ: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op1->val());
		rslt = builder().insert_add(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PADDD: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());
		rslt = builder().insert_add(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PADDW: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op1->val());
		rslt = builder().insert_add(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PADDB: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u8(), 16), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u8(), 16), op1->val());
		rslt = builder().insert_add(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PSUBQ: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u64(), 2), op1->val());
		rslt = builder().insert_sub(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PSUBD: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), op1->val());
		rslt = builder().insert_sub(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PSUBW: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u16(), 8), op1->val());
		rslt = builder().insert_sub(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PSUBB: {
		auto lhs = builder().insert_bitcast(value_type::vector(value_type::u8(), 16), op0->val());
		auto rhs = builder().insert_bitcast(value_type::vector(value_type::u8(), 16), op1->val());
		rslt = builder().insert_sub(lhs->val(), rhs->val());
		break;
	}
	case XED_ICLASS_PCMPEQB: {
    auto lhs = read_operand(0);
    auto rhs = read_operand(1);
    auto nr_bytes = lhs->val().type().width() == 64 ? 8 : 16;
    lhs = builder().insert_bitcast(value_type::vector(value_type::u8(), nr_bytes), lhs->val());
    rhs = builder().insert_bitcast(value_type::vector(value_type::u8(), nr_bytes), rhs->val());

    auto cst_0 = builder().insert_constant_u8(0x00);
    auto cst_1 = builder().insert_constant_u8(0xFF);

    for (int i = 0; i < nr_bytes; i++) {
      auto equal = builder().insert_cmpeq(builder().insert_vector_extract(lhs->val(), i)->val(), builder().insert_vector_extract(rhs->val(), i)->val());
      auto res = builder().insert_csel(equal->val(), cst_1->val(), cst_0->val());
      lhs = builder().insert_vector_insert(lhs->val(), i, res->val());
    }
    rslt = lhs;
    break;
  }

  case XED_ICLASS_PCMPGTB:
  case XED_ICLASS_PCMPGTW:
  case XED_ICLASS_PCMPGTD: {
    auto lhs = read_operand(0);
    auto rhs = read_operand(1);
    auto nr_splits = (inst_class == XED_ICLASS_PCMPGTB) ? 8 : (inst_class == XED_ICLASS_PCMPGTW) ? 4 : 2;
    if (lhs->val().type().width() == 128) {
      nr_splits *= 2;
    }
    auto typ = (inst_class == XED_ICLASS_PCMPGTB) ? value_type::u8() : (inst_class == XED_ICLASS_PCMPGTW) ? value_type::u16() : value_type::u32();
    lhs = builder().insert_bitcast(value_type::vector(typ, nr_splits), lhs->val());
    rhs = builder().insert_bitcast(value_type::vector(typ, nr_splits), rhs->val());

    auto cst_0 = builder().insert_constant_i(value_type(value_type_class::unsigned_integer, lhs->val().type().width() / nr_splits), 0);
    auto cst_1 = (inst_class == XED_ICLASS_PCMPGTB) ? builder().insert_constant_u8(0xFF) : (inst_class == XED_ICLASS_PCMPGTW) ? builder().insert_constant_u16(0xFFFF) : builder().insert_constant_u32(0xFFFFFFFF);

    for (int i = 0; i < nr_splits; i++) {
      auto gt = builder().insert_cmpgt(builder().insert_vector_extract(lhs->val(), i)->val(), builder().insert_vector_extract(rhs->val(), i)->val());
      auto res = builder().insert_csel(gt->val(), cst_1->val(), cst_0->val());
      lhs = builder().insert_vector_insert(lhs->val(), i, res->val());
    }
    rslt = lhs;

    break;
  }

	case XED_ICLASS_XADD: {
    auto dst = read_operand(0);
    auto src = read_operand(1);

    auto sum = builder().insert_add(src->val(), dst->val());

    write_flags(sum, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);

    write_operand(1, dst->val());
    write_operand(0, sum->val());
    break;
  }
	case XED_ICLASS_BT:
  case XED_ICLASS_BTS:
  case XED_ICLASS_BTR: {
    auto src = read_operand(0);
    auto pos = read_operand(1);

    pos = builder().insert_zx(src->val().type(), pos->val());
    pos = builder().insert_bitcast(src->val().type(), pos->val());
    auto shift = builder().insert_lsl(builder().insert_constant_i(pos->val().type(), 1)->val(), pos->val());
    auto and_node = builder().insert_and(src->val(), shift->val());
    auto rslt = builder().insert_lsr(and_node->val(), pos->val());
    rslt = builder().insert_trunc(value_type::u1(), rslt->val());

    write_reg(reg_offsets::CF, rslt->val());

    if (inst_class == XED_ICLASS_BTS) {
      auto or_node = builder().insert_or(src->val(), shift->val());
      write_operand(0, or_node->val());
    } else if (inst_class == XED_ICLASS_BTR) {
      auto and_not = builder().insert_and(src->val(), builder().insert_not(shift->val())->val());
      write_operand(0, and_not->val());
    }

		break;
  }
  case XED_ICLASS_BSR:
  case XED_ICLASS_BSF: {
    auto src = read_operand(1);
    auto type = src->val().type();

    // if src == 0
    auto src_zero = builder().insert_cmpeq(src->val(), builder().insert_constant_i(type, 0)->val());
    cond_br_node *br = (cond_br_node *)builder().insert_cond_br(src_zero->val(), nullptr);

    // then [src != 0]
    write_reg(reg_offsets::ZF, builder().insert_constant_i(value_type::u1(), 0)->val());
    auto mask = builder().alloc_local(type);
    builder().insert_write_local(mask, builder().insert_constant_i(type, 1)->val()); // mask := 0x1
    auto idx = builder().alloc_local(value_type::u64());
    if (inst_class == XED_ICLASS_BSR) {
      auto mask_val = builder().insert_read_local(mask);
      mask_val = builder().insert_lsl(mask_val->val(), builder().insert_constant_i(type, type.width() - 1)->val());
      builder().insert_write_local(mask, mask_val->val()); // mask := mask << width - 1
      builder().insert_write_local(idx, builder().insert_constant_u64(type.width() - 1)->val()); // idx = width - 1
    } else {
      builder().insert_write_local(idx, builder().insert_constant_u64(0)->val()); // idx := 0
    }
    //   while mask & src == 0; do idx--; mask = mask >> 1; done
    auto while_loop = builder().insert_label("while");

    //       mask & src == 1  -> jump to endif
    auto mask_val = builder().insert_read_local(mask);
    auto mask_and = builder().insert_and(mask_val->val(), src->val());
    cond_br_node *fi_br = (cond_br_node *)builder().insert_cond_br(mask_and->val(), nullptr);
    //       idx--/++; mask = mask >>/<< 1
    auto idx_val = builder().insert_read_local(idx);
    if (inst_class == XED_ICLASS_BSR) {
      idx_val = builder().insert_sub(idx_val->val(), builder().insert_constant_u64(1)->val());
      mask_val = builder().insert_lsr(mask_val->val(), builder().insert_constant_i(type, 1)->val());
    } else {
      idx_val = builder().insert_add(idx_val->val(), builder().insert_constant_u64(1)->val());
      mask_val = builder().insert_lsl(mask_val->val(), builder().insert_constant_i(type, 1)->val());
    }
    builder().insert_write_local(idx, idx_val->val());
    builder().insert_write_local(mask, mask_val->val());
    //       jump back to loop test
    builder().insert_br(while_loop);

    // else [src == 0]
    auto else_label = builder().insert_label("else");
    br->add_br_target(else_label);
    write_reg(reg_offsets::ZF, builder().insert_constant_i(value_type::u1(), 1)->val());

    // fi
    auto fi_label = builder().insert_label("fi");
    fi_br->add_br_target(fi_label);

    // write result
    write_operand(0, builder().insert_read_local(idx)->val());

    break;
  }
	case XED_ICLASS_COMISS: {
		// comiss op0 op1: compares the lowest fp32 value of op0 and op1 and sets EFLAGS such as:
		// op0 > op1: ZF = PF = CF = 0
		// op0 < op1: ZF = PF = 0, CF = 1
		// op0 = op1: ZF = 1, PF = CF = 0
		// op0 or op1 = NaN: ZF = PF = CF = 1

		// Only SSE version is supported
		auto op0 = read_operand(0);
		auto op1 = read_operand(1);

		op0 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op0->val());
		op0 = builder().insert_vector_extract(op0->val(), 0);
		if (op1->val().type().width() == 128) {
			op1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op1->val());
			op1 = builder().insert_vector_extract(op1->val(), 0);
		} else {
			op1 = builder().insert_bitcast(value_type::f32(), op1->val());
		}

		// op0 is NaN?
		auto op0_cast = builder().insert_bitcast(value_type::u32(), op0->val());
		auto and_exp = builder().insert_and(op0_cast->val(), builder().insert_constant_u32(0x7F800000)->val());
		auto cmpeq_exp = builder().insert_cmpeq(and_exp->val(), builder().insert_constant_u32(0)->val());
		cond_br_node *op0_not_nan_br = (cond_br_node *)builder().insert_cond_br(cmpeq_exp->val(), nullptr);
		auto and_frac = builder().insert_and(op0_cast->val(), builder().insert_constant_u32(0x7FFFFF)->val());
		auto cmpeq_frac = builder().insert_cmpeq(and_frac->val(), builder().insert_constant_u32(0)->val());
		cond_br_node *op0_not_nan_br2 = (cond_br_node *)builder().insert_cond_br(cmpeq_frac->val(), nullptr);
		write_flags(nullptr, flag_op::set1, flag_op::set1, flag_op::set0, flag_op::set0, flag_op::set1, flag_op::set0);
		br_node *end0nan_br = (br_node *)builder().insert_br(nullptr);

		// op1 is NaN?
		auto op1_cast = builder().insert_bitcast(value_type::u32(), op1->val());
		and_exp = builder().insert_and(op1_cast->val(), builder().insert_constant_u32(0x7F800000)->val());
		cmpeq_exp = builder().insert_cmpeq(and_exp->val(), builder().insert_constant_u32(0)->val());
		cond_br_node *op1_not_nan_br = (cond_br_node *)builder().insert_cond_br(cmpeq_exp->val(), nullptr);
		and_frac = builder().insert_and(op1_cast->val(), builder().insert_constant_u32(0x7FFFFF)->val());
		cmpeq_frac = builder().insert_cmpeq(and_frac->val(), builder().insert_constant_u32(0)->val());
		cond_br_node *op1_not_nan_br2 = (cond_br_node *)builder().insert_cond_br(cmpeq_frac->val(), nullptr);
		write_flags(nullptr, flag_op::set1, flag_op::set1, flag_op::set0, flag_op::set0, flag_op::set1, flag_op::set0);
		br_node *end1nan_br = (br_node *)builder().insert_br(nullptr);

		auto no_nan = builder().insert_label("not_NaN");
		op0_not_nan_br->add_br_target(no_nan);
		op0_not_nan_br2->add_br_target(no_nan);
		op1_not_nan_br->add_br_target(no_nan);
		op1_not_nan_br2->add_br_target(no_nan);
		auto cmpeq = builder().insert_cmpeq(op0->val(), op1->val());
		cond_br_node *eq_br = (cond_br_node *)builder().insert_cond_br(cmpeq->val(), nullptr);
		auto cmpgt = builder().insert_cmpgt(op0->val(), op1->val());
		cond_br_node *gt_br = (cond_br_node *)builder().insert_cond_br(cmpgt->val(), nullptr);

		// op0 < op1
		write_flags(nullptr, flag_op::set0, flag_op::set1, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0);
		br_node *endlt_br = (br_node *)builder().insert_br(nullptr);

		// equal
		auto eq_branch = builder().insert_label("equal");
		eq_br->add_br_target(eq_branch);
		write_flags(nullptr, flag_op::set1, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0);
		br_node *endeq_br = (br_node *)builder().insert_br(nullptr);

		// op0 > op1
		auto gt_branch = builder().insert_label("gt");
		gt_br->add_br_target(gt_branch);
		write_flags(nullptr, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0, flag_op::set0);
		br_node *endgt_br = (br_node *)builder().insert_br(nullptr);

		// end
		auto end = builder().insert_label("end");
		endeq_br->add_br_target(end);
		endgt_br->add_br_target(end);
		endlt_br->add_br_target(end);
		end0nan_br->add_br_target(end);
		end1nan_br->add_br_target(end);

		break;
	}
	default:
		throw std::runtime_error("unsupported binop");
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_CMP:
	case XED_ICLASS_TEST:
	case XED_ICLASS_BT:
	case XED_ICLASS_BTS:
	case XED_ICLASS_BTR:
	case XED_ICLASS_BSR:
  case XED_ICLASS_BSF:
	case XED_ICLASS_COMISS:
	case XED_ICLASS_XADD:
	case XED_ICLASS_ADDSD:
		break;

	default:
		write_operand(0, rslt->val());
		break;
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_AND:
	case XED_ICLASS_TEST:
	case XED_ICLASS_OR:
		write_flags(rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;

	case XED_ICLASS_ADD:
	case XED_ICLASS_ADC:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_CMP:
		write_flags(rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
		break;

	default:
		break;
	}
}
