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

  default:
	  throw std::runtime_error("unsupported fpu operation");
  }

  switch (inst_class) {
  case XED_ICLASS_FSTP:
  case XED_ICLASS_FUCOMIP:
	  fpu_stack_top_move(1);
    break;
  default:
    break;
  }
}
