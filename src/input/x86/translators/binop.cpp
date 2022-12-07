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

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
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
	case XED_ICLASS_XADD: {
        auto dst = read_operand(0);
        auto src = read_operand(1);

        auto sum = builder().insert_add(src->val(), dst->val());

        write_flags(sum, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);

        write_operand(1, dst->val());
        write_operand(0, sum->val());
        break;
    }
	case XED_ICLASS_BT: {
		auto src = read_operand(0);
		auto pos = read_operand(1);

		pos = builder().insert_zx(src->val().type(), pos->val());
		auto shift = builder().insert_lsl(builder().insert_constant_i(pos->val().type(), 1)->val(), pos->val());
		auto and_node = builder().insert_and(src->val(), shift->val());
		auto rslt = builder().insert_lsr(and_node->val(), pos->val());
		rslt = builder().insert_trunc(value_type::u1(), rslt->val());

		write_reg(reg_offsets::CF, rslt->val());
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
	case XED_ICLASS_COMISS:
	case XED_ICLASS_XADD:
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
