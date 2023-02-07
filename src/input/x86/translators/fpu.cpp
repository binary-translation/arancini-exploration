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
    dump_xed_encoding();
    // xed encoding: fchs st(0)
    auto st0 = read_operand(0);
    write_operand(0, builder().insert_not(st0->val())->val());

    // TODO flag management
    break;
  }

  case XED_ICLASS_FXCH: {
    dump_xed_encoding();
    // xed encoding: fxch st(i) st(j)
    // TODO fix with temp node
    auto sti = read_operand(0);
    auto stj = read_operand(1);

    write_operand(0, stj->val());
    write_operand(1, sti->val());

    // TODO set C1 flag to 0
    break;
  }

  default:
	  throw std::runtime_error("unsupported fpu operation");
  }

  switch (inst_class) {
  case XED_ICLASS_FSTP:
  case XED_ICLASS_FADDP:
  case XED_ICLASS_FSUBP:
  case XED_ICLASS_FUCOMIP:
	  fpu_stack_top_move(1);
    break;
  default:
    break;
  }
}
