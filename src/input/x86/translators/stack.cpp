#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void stack_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_PUSH: {
		auto rsp = read_reg(value_type::u64(), reg_offsets::RSP);
		auto new_rsp = builder().insert_sub(rsp->val(), builder().insert_constant_u64(8)->val());

		builder().insert_write_mem(new_rsp->val(), read_operand(0)->val());
		write_reg(reg_offsets::RSP, new_rsp->val());
		break;
	}

	case XED_ICLASS_POP: {
		auto rsp = read_reg(value_type::u64(), reg_offsets::RSP);
		write_operand(0, builder().insert_read_mem(value_type::u64(), rsp->val())->val());

		auto new_rsp = builder().insert_add(rsp->val(), builder().insert_constant_u64(8)->val());
		write_reg(reg_offsets::RSP, new_rsp->val());
		break;
	}

	case XED_ICLASS_LEAVE: {
		/* Only supported for 64-bit mode */
		/*
		* LEAVE:
		* 	RSP := RBP
		* 	RBP := Pop()
		*/
		auto rbp = read_reg(value_type::u64(), reg_offsets::RBP);
		write_reg(reg_offsets::RSP, rbp->val());

		auto rsp = read_reg(value_type::u64(), reg_offsets::RSP);
		write_reg(reg_offsets::RBP, builder().insert_read_mem(value_type::u64(), rsp->val())->val());

		auto new_rsp = builder().insert_add(rsp->val(), builder().insert_constant_u64(8)->val());
		write_reg(reg_offsets::RSP, new_rsp->val());
		break;
	}

	default:
		throw std::runtime_error("unsupported stack operation");
	}
}
