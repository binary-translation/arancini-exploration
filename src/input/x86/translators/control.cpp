#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/internal-function-resolver.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void control_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XGETBV: {
		// Reads an XCR specified by ECX into EDX:EAX
    // Only ECX == 0 is supported
    // xed encoding: xgetbv ecx edx eax xcr0

    auto xcr0 = read_reg(value_type::u64(), reg_offsets::XCR0);
    auto eax = builder().insert_bit_extract(xcr0->val(), 0, 32);
    auto edx = builder().insert_bit_extract(xcr0->val(), 32, 32);
    write_operand(1, edx->val());
    write_operand(2, eax->val());

    break;
  }

  case XED_ICLASS_STD:
    write_reg(reg_offsets::DF, builder().insert_constant_u1(1)->val());
    break;

  case XED_ICLASS_CLD:
    write_reg(reg_offsets::DF, builder().insert_constant_u1(0)->val());
    break;

  case XED_ICLASS_STC:
    write_reg(reg_offsets::CF, builder().insert_constant_u1(1)->val());
    break;

  case XED_ICLASS_CLC:
    write_reg(reg_offsets::CF, builder().insert_constant_u1(0)->val());
    break;
	default:
		throw std::runtime_error("unhandled control register instruction");
	}
}
