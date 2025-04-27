#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void branch_translator::do_translate() {
    switch (xed_decoded_inst_get_iclass(xed_inst())) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR: {
        // push next insn to stack, write target to pc
        auto rsp = read_reg(value_type::u64(), reg_offsets::RSP);
        auto new_rsp = builder().insert_sub(
            rsp->val(), builder().insert_constant_u64(8)->val());

        write_reg(reg_offsets::RSP, new_rsp->val());

        xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst());
        auto next_target_node = builder().insert_add(
            builder().insert_read_pc()->val(),
            builder().insert_constant_u64(instruction_length)->val());
        builder().insert_write_mem(new_rsp->val(), next_target_node->val());

        auto target = read_operand(0);

        if (target->kind() == ir::node_kinds::constant) {
            auto const_val = ((constant_node *)target)->const_val_i();
            auto call_target = builder().insert_add(
                target->val(),
                builder().insert_constant_u64(instruction_length)->val());
            target = builder().insert_add(builder().insert_read_pc()->val(),
                                          call_target->val());
            builder().insert_write_pc(target->val(), br_type::call,
                                      const_val + instruction_length);
            break;
        }

        builder().insert_write_pc(target->val(), br_type::call);
        break;
    }

    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR: {
        // pop stack, write to pc

        auto rsp = read_reg(value_type::u64(), reg_offsets::RSP);
        auto retaddr = builder().insert_read_mem(value_type::u64(), rsp->val());

        auto new_rsp = builder().insert_add(
            rsp->val(), builder().insert_constant_u64(8)->val());
        write_reg(reg_offsets::RSP, new_rsp->val());

        builder().insert_write_pc(retaddr->val(), br_type::ret);

        break;
    }

    case XED_ICLASS_JMP: {
        xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst());
        auto target = read_operand(0);

        if (target->kind() == ir::node_kinds::constant) {
            auto branch_target = builder().insert_add(
                target->val(),
                builder().insert_constant_u64(instruction_length)->val());
            target = builder().insert_add(builder().insert_read_pc()->val(),
                                          branch_target->val());
        }

        builder().insert_write_pc(target->val(), br_type::br);

        break;
    }

    default:
        throw std::runtime_error("unsupported branch operation");
    }
}
