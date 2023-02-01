#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void fpu_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {

  case XED_ICLASS_FNSTCW: {
    auto fpu_ctrl = read_reg(value_type::u16(), reg_offsets::X87CTRL);
    write_operand(0, fpu_ctrl->val());
    break;
  }

  default:
	  throw std::runtime_error("unsupported fpu operation");
  }
}
