#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void stack_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_PUSH: {
		auto rsp = read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		auto new_rsp = pkt()->insert_sub(rsp->val(), pkt()->insert_constant_u64(8)->val());

		write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());
		pkt()->insert_write_mem(new_rsp->val(), read_operand(0)->val());
		break;
	}

	case XED_ICLASS_POP: {
		auto rsp = read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		write_operand(0, pkt()->insert_read_mem(value_type::u64(), rsp->val())->val());

		auto new_rsp = pkt()->insert_add(rsp->val(), pkt()->insert_constant_u64(8)->val());
		write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());
		break;
	}

	default:
		throw std::runtime_error("unsupported stack operation");
	}
}
