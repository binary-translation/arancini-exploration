#include "type-utils.h"

#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/input/registers.h>
#include <arancini/input/input-arch.h>
#include <arancini/input/x86/translators/translators.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = auto_cast(op0->val().type(), read_operand(1));

	value_node *rslt = nullptr;

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
	case XED_ICLASS_ANDPS: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op1->val());
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_ANDNPS: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op0->val());
		op0 = builder().insert_not(op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op1->val());
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_ANDPD: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op1->val());
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_ANDNPD: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op0->val());
		op0 = builder().insert_not(op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op1->val());
		rslt = builder().insert_and(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_OR:
	case XED_ICLASS_POR:
		rslt = builder().insert_or(op0->val(), op1->val());
		break;
	case XED_ICLASS_ORPS: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op1->val());
		rslt = builder().insert_or(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_ORPD: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f64(), 2), op1->val());
		rslt = builder().insert_or(op0->val(), op1->val());
		break;
	}
	case XED_ICLASS_ADD:
		rslt = builder().insert_add(op0->val(), op1->val());
		break;
	case XED_ICLASS_ADDPS: {
		op0 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op0->val());
		op1 = builder().insert_bitcast(value_type::vector(value_type::f32(), 4), op1->val());
		rslt = builder().insert_add(op0->val(), op1->val());
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
	case XED_ICLASS_PCMPEQB:
	case XED_ICLASS_PCMPEQW:
	case XED_ICLASS_PCMPEQD: {
    auto lhs = read_operand(0);
    auto rhs = read_operand(1);
    auto nr_bits = lhs->val().type().width();
	auto ty = value_type::v();
	value_node *cst_0;
	value_node *cst_1;

	switch (inst_class) {
		case XED_ICLASS_PCMPEQB: {
			ty = value_type::u8();
			cst_0 = builder().insert_constant_u8(0x00);
			cst_1 = builder().insert_constant_u8(0xFF);
		} break;
		case XED_ICLASS_PCMPEQW: {
			ty = value_type::u16();
			cst_0 = builder().insert_constant_u16(0x0000);
			cst_1 = builder().insert_constant_u16(0xFFFF);
		} break;
		case XED_ICLASS_PCMPEQD: {
			ty = value_type::u32();
			cst_0 = builder().insert_constant_u32(0x00000000);
			cst_1 = builder().insert_constant_u32(0xFFFFFFFF);
		} break;
		default: throw std::runtime_error("unhandled case in PCMPEQX");
	}

    lhs = builder().insert_bitcast(value_type::vector(ty, nr_bits/ty.width()), lhs->val());
    rhs = builder().insert_bitcast(value_type::vector(ty, nr_bits/ty.width()), rhs->val());

    for (std::size_t i = 0; i < nr_bits/ty.width(); i++) {
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

    auto idx = builder().alloc_local(value_type::u64());
    auto mask = builder().alloc_local(type);
    // if src == 0
    auto src_zero = builder().insert_cmpeq(src->val(), builder().insert_constant_i(type, 0)->val());
    cond_br_node *undef = (cond_br_node *)builder().insert_cond_br(src_zero->val(), nullptr);

    // then [src != 0]
    write_reg(reg_offsets::ZF, builder().insert_constant_i(value_type::u1(), 0)->val());
    builder().insert_write_local(mask, builder().insert_constant_i(type, 1)->val()); // mask := 0x1
    if (inst_class == XED_ICLASS_BSR) {
      auto mask_val = builder().insert_read_local(mask);
      mask_val = builder().insert_lsl(mask_val->val(), builder().insert_constant_i(type, type.width() - 1)->val());
      builder().insert_write_local(mask, mask_val->val()); // mask := mask << width - 1
      builder().insert_write_local(idx, builder().insert_constant_u64(type.width() - 1)->val()); // idx = width - 1
    } else {
      builder().insert_write_local(idx, builder().insert_constant_u64(0)->val()); // idx := 0
    }
	auto next = (br_node *)builder().insert_br(nullptr);
    //   while mask & src == 0; do idx--; mask = mask >> 1; done
	auto while_loop1 = builder().insert_label("while");
	next->add_br_target(while_loop1);
	auto while_loop = builder().insert_label("while");

	//       mask & src == 1  -> jump to endif
    auto mask_val = builder().insert_read_local(mask);
    auto mask_and = builder().insert_cmpne(builder().insert_and(mask_val->val(), src->val())->val(), builder().insert_constant_i(type, 0)->val());
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
    undef->add_br_target(else_label);
    write_reg(reg_offsets::ZF, builder().insert_constant_i(value_type::u1(), 1)->val());
	// undef, so write 0
    write_operand(0, builder().insert_constant_i(type, 0)->val());
	auto end = (br_node *)builder().insert_br(nullptr);

    // fi
    auto fi_label = builder().insert_label("fi");
    fi_br->add_br_target(fi_label);

    // write result
    write_operand(0, builder().insert_read_local(idx)->val());
	next = (br_node *)builder().insert_br(nullptr);

	auto end_label = builder().insert_label("end");
	auto end_label2 = builder().insert_label("end");
    next->add_br_target(end_label);
    end->add_br_target(end_label2);


    break;
  }
	case XED_ICLASS_COMISS:
	case XED_ICLASS_COMISD:
	case XED_ICLASS_UCOMISS:
	case XED_ICLASS_UCOMISD: {
		// comiss op0 op1: compares the lowest fp32 value of op0 and op1 and sets EFLAGS such as:
		// op0 > op1: ZF = PF = CF = 0
		// op0 < op1: ZF = PF = 0, CF = 1
		// op0 = op1: ZF = 1, PF = CF = 0
		// op0 or op1 = NaN: ZF = PF = CF = 1

		// Only SSE version is supported

		// The flags for ucomisd are the same, execptions would be different
		auto op0 = read_operand(0);
		auto op1 = read_operand(1);

		value_type ETy = value_type::v();
        // TODO: is this needed?
		// value_type CastTy = value_type::v();
		int ENum;

        // TODO: are these needed?
		// value_node *cexp;
		// value_node *cfrac;
		// value_node *z;

		switch(inst_class) {
			case XED_ICLASS_UCOMISS:
			case XED_ICLASS_COMISS: {
				ETy = value_type::f32();
				ENum = 4;
			} break;
			case XED_ICLASS_COMISD:
			case XED_ICLASS_UCOMISD: {
				ETy = value_type::f64();
				ENum = 2;
			} break;
			default: break;
		}

		op0 = builder().insert_bitcast(value_type::vector(ETy, ENum), op0->val());
		op0 = builder().insert_vector_extract(op0->val(), 0);
		if (op1->val().type().width() == 128) {
			op1 = builder().insert_bitcast(value_type::vector(ETy, ENum), op1->val());
			op1 = builder().insert_vector_extract(op1->val(), 0);
		} else {
			op1 = builder().insert_bitcast(ETy, op1->val());
		}

		auto is_nan = builder().insert_binop(binary_arith_op::cmpu, op0->val(), op1->val());

		auto is_nan_or_eq = builder().insert_binop(binary_arith_op::cmpueq, op0->val(), op1->val());

		auto is_nan_or_lt = builder().insert_binop(binary_arith_op::cmpult, op0->val(), op1->val());

		builder().insert_write_reg(
			util::to_underlying(reg_offsets::ZF),
			util::to_underlying(reg_idx::ZF),
			"ZF",
			builder().insert_csel(is_nan_or_eq->val(),
				builder().insert_constant_i(value_type::u1(), 1)->val(),
				builder().insert_constant_i(value_type::u1(), 0)->val()
			)->val()
		);

		builder().insert_write_reg(
			util::to_underlying(reg_offsets::PF),
			util::to_underlying(reg_idx::PF),
			"PF",
			builder().insert_csel(is_nan->val(),
				builder().insert_constant_i(value_type::u1(), 1)->val(),
				builder().insert_constant_i(value_type::u1(), 0)->val()
			)->val()
		);

		builder().insert_write_reg(
			util::to_underlying(reg_offsets::CF),
			util::to_underlying(reg_idx::CF),
			"CF",
			builder().insert_csel(is_nan_or_lt->val(),
				builder().insert_constant_i(value_type::u1(), 1)->val(),
				builder().insert_constant_i(value_type::u1(), 0)->val()
			)->val()
		);

		write_flags(nullptr, flag_op::ignore, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::set0);
		break;
	}
	case XED_ICLASS_CMPSD_XMM:
	case XED_ICLASS_CMPSS: {
		auto dest = read_operand(0);
		auto op0 = read_operand(0);
		auto op1 = read_operand(1);
		auto op2 = reinterpret_cast<constant_node *>(read_operand(2));

		value_node *true_val = nullptr;
		value_node *false_val = nullptr;

		value_type vector_type = value_type::v();

		switch (inst_class) {
			case XED_ICLASS_CMPSS: {
				true_val = builder().insert_constant_f32(::util::bit_cast_zeros<float>(0xFFFFFFFF));
				false_val = builder().insert_constant_f32(::util::bit_cast_zeros<float>(0x00000000));
				vector_type = value_type::vector(value_type::f32(), 4);
			} break;
			case XED_ICLASS_CMPSD_XMM: {
				true_val = builder().insert_constant_f64(::util::bit_cast_zeros<double>(0xFFFFFFFFFFFFFFFF));
				false_val = builder().insert_constant_f64(::util::bit_cast_zeros<double>(0x000000000000000));
				vector_type = value_type::vector(value_type::f64(), 2);
			} break;
            default:
                throw frontend_exception("Unhandled XED instruction class");
		}

        // TODO: is it needed?
		// auto mask = builder().insert_constant_u8(3);

		dest = builder().insert_bitcast(vector_type, dest->val());
		op0 = builder().insert_bitcast(vector_type, op0->val());
		op0 = builder().insert_vector_extract(op0->val(), 0);
        if (inst_class == XED_ICLASS_CMPSD_XMM && !op1->val().type().is_vector()) {
            op1 = builder().insert_bitcast(value_type::f64(), op1->val());
        } else {
            op1 = builder().insert_bitcast(vector_type, op1->val());
            op1 = builder().insert_vector_extract(op1->val(), 0);
        }

		auto op_code = op2->const_val_i();

		/*
		 * 0 - ordered eq
		 * 1 - ordered lt
		 * 2 - ordered le
		 * 3 - unordered
		 * 4 - unordered ne
		 * 5 - unordered nlt
		 * 6 - unordered nle
		 * 7 - ordered
		 */

		value_node *cond = nullptr;
		switch (op_code) {
			case 0: cond = builder().insert_binop(binary_arith_op::cmpoeq, op0->val(), op1->val()); break;
			case 1: cond = builder().insert_binop(binary_arith_op::cmpolt, op0->val(), op1->val()); break;
			case 2: cond = builder().insert_binop(binary_arith_op::cmpole, op0->val(), op1->val()); break;
			case 3: cond = builder().insert_binop(binary_arith_op::cmpu, op0->val(), op1->val()); break;
			case 4: cond = builder().insert_binop(binary_arith_op::cmpune, op0->val(), op1->val()); break;
			case 5: cond = builder().insert_binop(binary_arith_op::cmpunlt, op0->val(), op1->val()); break;
			case 6: cond = builder().insert_binop(binary_arith_op::cmpunle, op0->val(), op1->val()); break;
			case 7: cond = builder().insert_binop(binary_arith_op::cmpo, op0->val(), op1->val()); break;
            default:
                throw frontend_exception("Invalid operation code for CMPSS");
		}
		auto res = builder().insert_csel(cond->val(), true_val->val(), false_val->val());
		write_operand(0, builder().insert_vector_insert(dest->val(), 0, res->val())->val());
	} break;
	/*
	case XED_ICLASS_MAXSS:
	case XED_ICLASS_MAXSD:
	case XED_ICLASS_MINSS:
	case XED_ICLASS_MINSD: {
	auto src0 = read_operand(0);
	auto src1 = read_operand(1);

	auto VecTy = value_type::v();

	value_node *cexp;
	value_node *cfrac;
	value_node *z;



	switch(inst_class) {
		case XED_ICLASS_MAXSS:
		case XED_ICLASS_MINSS: {
			VecTy = value_type::vector(value_type::f32(), 4);
			cexp = builder().insert_constant_f32(static_cast<float>(0x7F8000000));
			cfrac = builder().insert_constant_f32(static_cast<float>(0x7FFFFF));
			z = builder().insert_constant_f32(static_cast<float>(0));
		} break;
		case XED_ICLASS_MAXSD:
		case XED_ICLASS_MINSD: {
			VecTy = value_type::vector(value_type::f64(), 2);
			cexp = builder().insert_constant_f64(static_cast<double>(0x7FF0000000000000));
			cfrac = builder().insert_constant_f64(static_cast<double>(0x000FFFFFFFFFFFFF));
			z = builder().insert_constant_f64(static_cast<double>(0));
		} break;
		default: break;
	}
	src0 = builder().insert_bitcast(VecTy, src0->val());
	src0 = builder().insert_vector_extract(src0->val(), 0);

	if (src1->val().type().width() == 128) {
		src1 = builder().insert_bitcast(VecTy, src1->val());
		src1 = builder().insert_vector_extract(src1->val(), 0);
	} else {
		src1 = builder().insert_bitcast(VecTy.element_type(), src1->val());
	}


	auto next = (br_node *)builder().insert_br(nullptr);
	auto do_checks = builder().insert_label("do_checks");
	next->add_br_target(do_checks);

	auto cnd = builder().insert_cmpne(z->val(), src0->val());
	auto cmp0 = (cond_br_node *)builder().insert_cond_br(cnd->val(), nullptr);
	cnd = builder().insert_cmpeq(z->val(), src1->val());
	auto both_0 = (cond_br_node *)builder().insert_cond_br(cnd->val(), nullptr);
	next = (br_node *)builder().insert_br(nullptr);

	auto check_for_nan = builder().insert_label("check_for_nan");
	auto check_for_nan1 = builder().insert_label("check_for_nan");
	next->add_br_target(check_for_nan);
	cmp0->add_br_target(check_for_nan1);

	auto and_exp = builder().insert_bitcast(VecTy.element_type(), builder().insert_and(src0->val(), cexp->val())->val());
	auto cmpeq_exp = builder().insert_cmpeq(and_exp->val(), cexp->val());
	auto src0_not_nan = (cond_br_node *)builder().insert_cond_br(cmpeq_exp->val(), nullptr);
	auto and_frac = builder().insert_bitcast(VecTy.element_type(), builder().insert_and(src0->val(), cfrac->val())->val());
	auto cmpeq_frac = builder().insert_cmpne(and_frac->val(), z->val());
	auto src0_is_nan = (cond_br_node *)builder().insert_cond_br(cmpeq_frac->val(), nullptr);
	next = (br_node *)builder().insert_br(nullptr);

	auto src0_nn = builder().insert_label("src0_not_nan");
	auto src0_nn1 = builder().insert_label("src0_not_nan");
	next->add_br_target(src0_nn);
	src0_not_nan->add_br_target(src0_nn1);

	and_exp = builder().insert_bitcast(VecTy.element_type(), builder().insert_and(src1->val(), cexp->val())->val());
	cmpeq_exp = builder().insert_cmpeq(and_exp->val(), cexp->val());
	auto src1_not_nan = (cond_br_node *)builder().insert_cond_br(cmpeq_exp->val(), nullptr);
	and_frac = builder().insert_bitcast(VecTy.element_type(), builder().insert_and(src1->val(), cfrac->val())->val());
	cmpeq_frac = builder().insert_cmpne(and_frac->val(), z->val());
	auto src1_is_nan = (cond_br_node *)builder().insert_cond_br(cmpeq_frac->val(), nullptr);
	next = (br_node *)builder().insert_br(nullptr);

	auto src1_nn = builder().insert_label("src1_not_nan");
	auto src1_nn1 = builder().insert_label("src1_not_nan");
	next->add_br_target(src1_nn);
	src1_not_nan->add_br_target(src1_nn1);

	value_node *src0_lt_src1;
	if (inst_class == XED_ICLASS_MINSD | inst_class == XED_ICLASS_MINSS)
		src0_lt_src1 = builder().insert_cmpgt(src1->val(), src0->val());
	else
		src0_lt_src1 = builder().insert_cmpgt(src0->val(), src1->val());
	auto src0_is_min = (cond_br_node *)builder().insert_cond_br(src0_lt_src1->val(), nullptr);
	next = (br_node *)builder().insert_br(nullptr);

	auto src1_out = builder().insert_label("src1_out");
	auto src1_out1 = builder().insert_label("src1_out");
	auto src1_out2 = builder().insert_label("src1_out");
	auto src1_out3 = builder().insert_label("src1_out");
	next->add_br_target(src1_out);
	both_0->add_br_target(src1_out1);
	src0_is_nan->add_br_target(src1_out2);
	src1_is_nan->add_br_target(src1_out3);
	write_operand(0, src1->val());
	auto end0 = (br_node *)builder().insert_br(nullptr);

	auto src0_out = builder().insert_label("src0_out");
	src0_is_min->add_br_target(src0_out);
	write_operand(0, src0->val());
	auto end1 = (br_node *)builder().insert_br(nullptr);

	auto end = builder().insert_label("end");
	auto endl1 = builder().insert_label("end");
	end0->add_br_target(end);
	end1->add_br_target(endl1);


  } break;
	*/
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
	case XED_ICLASS_COMISD:
	case XED_ICLASS_UCOMISS:
	case XED_ICLASS_UCOMISD:
	case XED_ICLASS_CMPSS:
	case XED_ICLASS_CMPSD_XMM:
	case XED_ICLASS_XADD:
	case XED_ICLASS_ADDSS:
	case XED_ICLASS_ADDSD:
	case XED_ICLASS_MINSD:
	case XED_ICLASS_MINSS:
	case XED_ICLASS_MAXSD:
	case XED_ICLASS_MAXSS:
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
