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


  case XED_ICLASS_FST:
  case XED_ICLASS_FSTP: {
    dump_xed_encoding();
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
