#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void branch_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_CALL_FAR:
	case XED_ICLASS_CALL_NEAR: {
		// push next insn to stack, write target to pc
		auto rsp = read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		auto new_rsp = builder().insert_sub(rsp->val(), builder().insert_constant_u64(8)->val());

		write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());

		xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst());
		auto next_target_node = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(instruction_length)->val());
		builder().insert_write_mem(new_rsp->val(), next_target_node->val());

		int32_t value = xed_decoded_inst_get_branch_displacement(xed_inst());
		uint64_t target = value + instruction_length;

		auto target_node = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(target)->val());

		builder().insert_write_pc(target_node->val());
		break;
	}

	case XED_ICLASS_RET_FAR:
	case XED_ICLASS_RET_NEAR: {
		// pop stack, write to pc

		auto rsp = read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		auto retaddr = builder().insert_read_mem(value_type::u64(), rsp->val());

		auto new_rsp = builder().insert_add(rsp->val(), builder().insert_constant_u64(8)->val());
		write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());

		builder().insert_write_pc(retaddr->val());

		break;
	}

	case XED_ICLASS_JMP: {
		xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst());
		int32_t branch_displacement = xed_decoded_inst_get_branch_displacement(xed_inst());
		uint64_t branch_target = branch_displacement + instruction_length;

		auto target = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(branch_target)->val());

		builder().insert_write_pc(target->val());

		break;
	}

	default:
		throw std::runtime_error("unsupported branch operation");
	}
}
