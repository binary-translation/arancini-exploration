#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void fpu_translator::do_translate()
{
	auto inst_class = xed_decoded_inst_get_iclass(xed_inst());

  switch (inst_class) {
	case XED_ICLASS_FNSTCW: {
		auto fpu_ctrl = read_reg(value_type::u16(), reg_offsets::X87_CTRL);
		write_operand(0, fpu_ctrl->val());
		break;
  }

  case XED_ICLASS_FLDCW: {
    auto src = read_operand(0);
    write_reg(reg_offsets::X87_CTRL, src->val());
    break;
  }

  case XED_ICLASS_FNSTSW: {
    auto fpu_status = read_reg(value_type::u16(), reg_offsets::X87_STS);
    write_operand(0, fpu_status->val());
    break;
  }

  case XED_ICLASS_FLD: {
    auto val = read_operand(0);

    if (val->val().type().width() != 80) {
      val = builder().insert_convert(value_type::f80(), val->val());
    }

    // push on the stack
    fpu_stack_top_move(-1);
    fpu_stack_set(0, val->val());

    // TODO flag management

    break;
  }

  case XED_ICLASS_FILD: {
    // xed encoding: fild st0 memint
    auto val = read_operand(1);

    if (val->val().type().width() != 80) {
      val = builder().insert_convert(value_type::f80(), val->val());
    }

    fpu_stack_top_move(-1);
    fpu_stack_set(0, val->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FLD: {
    auto val = read_operand(0);

    if (val->val().type().width() != 80) {
      val = builder().insert_convert(value_type::f80(), val->val());
    }

    // push on the stack
    fpu_stack_top_move(-1);
    fpu_stack_set(0, val->val());

    // TODO flag management

    break;
  }

  case XED_ICLASS_FILD: {
    // xed encoding: fild st0 memint
    auto val = read_operand(1);

    if (val->val().type().width() != 80) {
      val = builder().insert_convert(value_type::f80(), val->val());
    }

    fpu_stack_top_move(-1);
    fpu_stack_set(0, val->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FST:
  case XED_ICLASS_FSTP: {
    auto target_width = get_operand_width(0);

    // get stack top
    auto val = fpu_stack_get(0);

    // convert and write to memory
    if (target_width != 80) {
      val = builder().insert_convert(value_type(value_type_class::floating_point, target_width), val->val());
    }
    write_operand(0, val->val());

    // TODO flag management

    break;
  }

  case XED_ICLASS_FIST:
  case XED_ICLASS_FISTP: {
    auto st0 = fpu_stack_get(0);
    auto val = builder().insert_convert(value_type(value_type_class::signed_integer, get_operand_width(0)), st0->val());
    write_operand(0, val->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FADD:
  case XED_ICLASS_FADDP: {
    // xed encoding: fadd st(0) st(i)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_add(dst->val(), src->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FSUB:
  case XED_ICLASS_FSUBP: {
    // xed encoding: fsub st(0) st(i)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_sub(dst->val(), src->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FSUBR:
  case XED_ICLASS_FSUBRP: {
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_sub(src->val(), dst->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FISUB: {
    // xed encoding: fisub st(0) memint
    auto st0 = read_operand(0);
    auto op = read_operand(1);
    auto conv = builder().insert_convert(st0->val().type(), op->val());
    auto res = builder().insert_sub(st0->val(), conv->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FMUL:
  case XED_ICLASS_FMULP: {
    // xed encoding: fmul st(i) st(j)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_mul(dst->val(), src->val());
    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FDIV:
  case XED_ICLASS_FDIVP: {
    // xed encoding: fdiv st(i) st(j)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_div(dst->val(), src->val());
    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FPREM: {
    // xed encoding: fprem st(0) st(1)
    auto st0 = read_operand(0);
    auto st1 = read_operand(1);

    auto res = builder().insert_mod(st0->val(), st1->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FADD:
  case XED_ICLASS_FADDP: {
    // xed encoding: fadd st(0) st(i)
    auto dst = read_operand(0);
    auto src = read_operand(1);
    auto res = builder().insert_add(dst->val(), src->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FSUB:
  case XED_ICLASS_FSUBP: {
    // xed encoding: fsub st(0) st(i)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_sub(dst->val(), src->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FISUB: {
    // xed encoding: fisub st(0) memint
    auto st0 = read_operand(0);
    auto op = read_operand(1);
    auto conv = builder().insert_convert(st0->val().type(), op->val());
    auto res = builder().insert_sub(st0->val(), conv->val());

    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FMUL:
  case XED_ICLASS_FMULP: {
    // xed encoding: fmul st(i) st(j)
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 80) {
      src = builder().insert_convert(dst->val().type(), src->val());
    }

    auto res = builder().insert_mul(dst->val(), src->val());
    write_operand(0, res->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FLDZ: {
    auto zero = builder().insert_constant_f(value_type(value_type_class::floating_point, 80), +0.0);
    fpu_stack_top_move(-1);
    fpu_stack_set(0, zero->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FLD1: {
    auto one = builder().insert_constant_f(value_type(value_type_class::floating_point, 80), +1.0);
    fpu_stack_top_move(-1);
    fpu_stack_set(0, one->val());

    // TODO FPU flags
    break;
  }

  case XED_ICLASS_FCHS: {
    // xed encoding: fchs st(0)
    auto st0 = read_operand(0);
    write_operand(0, builder().insert_not(st0->val())->val());

    // TODO flag management
    break;
  }

  case XED_ICLASS_FABS: {
    auto st0 = read_operand(0);
    st0 = builder().insert_bit_insert(st0->val(), builder().insert_constant_u1(1)->val(), 79, 1);
    write_operand(0, st0->val());

    // TODO set C1 flag to 0
    break;
  }

  case XED_ICLASS_FXCH: {
    // xed encoding: fxch st(i) st(j)
    // TODO fix with temp node
    auto sti = read_operand(0);
    auto stj = read_operand(1);

    write_operand(0, stj->val());
    write_operand(1, sti->val());

    // TODO set C1 flag to 0
    break;
  }

  case XED_ICLASS_FCOMI:
  case XED_ICLASS_FCOMIP:
  case XED_ICLASS_FUCOMI:
  case XED_ICLASS_FUCOMIP: {
    // xed encoding: fucomi(p) st(0), st(i)
    // TODO properly manage the unordered case (SNan, QNaN, etc), and diff with F* and FU*

    // get stack top and operand
    auto st0 = read_operand(0);
    auto sti = read_operand(1);

    // comparisons
    auto cmpgt = builder().insert_cmpgt(st0->val(), sti->val());
    cond_br_node *gt_br = (cond_br_node *)builder().insert_cond_br(cmpgt->val(), nullptr);
    auto cmplt = builder().insert_cmpgt(sti->val(), st0->val());
    cond_br_node *lt_br = (cond_br_node *)builder().insert_cond_br(cmplt->val(), nullptr);
    auto cmpeq = builder().insert_cmpeq(st0->val(), sti->val());
    cond_br_node *eq_br = (cond_br_node *)builder().insert_cond_br(cmpeq->val(), nullptr);

    // unordered
    write_flags(nullptr, flag_op::set1, flag_op::set1, flag_op::ignore, flag_op::ignore, flag_op::set1, flag_op::ignore);
    br_node *end_un = (br_node *)builder().insert_br(nullptr);

    // st(0) > st(i)
    auto gt_branch = builder().insert_label("gt");
    gt_br->add_br_target(gt_branch);
    write_flags(nullptr, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::set0, flag_op::ignore);
    br_node *end_gt = (br_node *)builder().insert_br(nullptr);

    // st(0) < st(i)
    auto lt_branch = builder().insert_label("lt");
    lt_br->add_br_target(lt_branch);
    write_flags(nullptr, flag_op::set0, flag_op::set1, flag_op::ignore, flag_op::ignore, flag_op::set0, flag_op::ignore);
    br_node *end_lt = (br_node *)builder().insert_br(nullptr);

    // st(0) = st(i)
    auto eq_branch = builder().insert_label("eq");
    eq_br->add_br_target(eq_branch);
    write_flags(nullptr, flag_op::set0, flag_op::set1, flag_op::ignore, flag_op::ignore, flag_op::set0, flag_op::ignore);

    // end
    auto end = builder().insert_label("end");
    end_un->add_br_target(end);
    end_gt->add_br_target(end);
    end_lt->add_br_target(end);

    break;
  }

  default:
	  throw std::runtime_error("unsupported fpu operation");
  }

  switch (inst_class) {
  case XED_ICLASS_FSTP:
  case XED_ICLASS_FISTP:
  case XED_ICLASS_FADDP:
  case XED_ICLASS_FSUBP:
  case XED_ICLASS_FSUBRP:
  case XED_ICLASS_FMULP:
  case XED_ICLASS_FDIVP:
  case XED_ICLASS_FCOMIP:
  case XED_ICLASS_FUCOMIP:
	  fpu_stack_top_move(1);
    break;
  default:
    break;
  }
}
