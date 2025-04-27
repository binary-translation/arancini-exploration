#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/output/dynamic/x86/x86-translation-context.h>
#include <iostream>

extern "C" {
#include <xed/xed-interface.h>
}

using namespace arancini::output::dynamic::x86;
using namespace arancini::ir;

void x86_translation_context::begin_block() {
    std::cerr << "INPUT ASSEMBLY:" << std::endl;
    // builder_.int3();
}

void x86_translation_context::begin_instruction(off_t address,
                                                const std::string &disasm) {
    instruction_index_to_guest_[builder_.nr_instructions()] = address;

    this_pc_ = address;
    std::cerr << "  " << std::hex << address << ": " << disasm << std::endl;
}

void x86_translation_context::end_instruction() {}

void x86_translation_context::end_block() {
    do_register_allocation();
    builder_.xor_(
        x86_operand(x86_physical_register_operand(x86_register_names::AX), 32),
        x86_operand(x86_physical_register_operand(x86_register_names::AX), 32));
    builder_.ret();

    builder_.dump(std::cerr);

    builder_.emit(writer());

    std::cerr << "OUTPUT ASSEMBLY:" << std::endl;

    const unsigned char *code = (const unsigned char *)writer().ptr();
    size_t code_size = writer().size();

    off_t base_address = (off_t)code;
    size_t offset = 0;
    while (offset < code_size) {
        xed_decoded_inst_t xedd;
        xed_decoded_inst_zero(&xedd);
        xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64,
                                  XED_ADDRESS_WIDTH_64b);
        xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_ALL);

        xed_error_enum_t xed_error =
            xed_decode(&xedd, &code[offset], code_size - offset);
        if (xed_error != XED_ERROR_NONE) {
            // throw std::runtime_error("unable to decode instruction: " +
            // std::to_string(xed_error));
            break;
        }

        xed_uint_t length = xed_decoded_inst_get_length(&xedd);

        char buffer[64];
        xed_format_context(XED_SYNTAX_INTEL, &xedd, buffer, sizeof(buffer) - 1,
                           base_address, nullptr, 0);
        std::cerr << "  " << std::hex << base_address << ": " << buffer
                  << std::endl;

        offset += length;
        base_address += length;
    }
}

void x86_translation_context::lower(const std::shared_ptr<action_node> &n) {
    materialise(n.get());
}

void x86_translation_context::materialise(node *n) {
    if (materialised_nodes_.count(n)) {
        return;
    }

    switch (n->kind()) {
    case node_kinds::read_reg:
        materialise_read_reg((read_reg_node *)n);
        break;
    case node_kinds::binary_arith:
        materialise_binary_arith((binary_arith_node *)n);
        break;
    case node_kinds::cast:
        materialise_cast((cast_node *)n);
        break;
    case node_kinds::write_reg:
        materialise_write_reg((write_reg_node *)n);
        break;
    case node_kinds::constant:
        materialise_constant((constant_node *)n);
        break;
    case node_kinds::read_mem:
        materialise_read_mem((read_mem_node *)n);
        break;
    case node_kinds::write_mem:
        materialise_write_mem((write_mem_node *)n);
        break;
    case node_kinds::read_pc:
        materialise_read_pc((read_pc_node *)n);
        break;
    case node_kinds::write_pc:
        materialise_write_pc((write_pc_node *)n);
        break;
    default:
        throw std::runtime_error("unsupported node");
    }

    materialised_nodes_.insert(n);
}

static x86_operand virtreg_operand(unsigned int index, int width) {
    return x86_operand(x86_virtual_register_operand(index),
                       width == 1 ? 8 : width);
}

static x86_operand imm_operand(unsigned long value, int width) {
    return x86_operand(x86_immediate_operand(value), width == 1 ? 8 : width);
}

static x86_operand guestreg_memory_operand(int width, int regoff) {
    return x86_operand(x86_memory_operand(x86_register_names::BP, regoff),
                       width == 1 ? 8 : width);
}

x86_operand x86_translation_context::vreg_operand_for_port(port &p,
                                                           bool constant_fold) {
    if (0 && constant_fold) {
        if (p.owner()->kind() == node_kinds::read_pc) {
            return x86_operand(x86_immediate_operand(this_pc_), 64);
        } else if (p.owner()->kind() == node_kinds::constant) {
            return x86_operand(x86_immediate_operand(
                                   ((constant_node *)p.owner())->const_val_i()),
                               p.type().width());
        }
    }

    materialise(p.owner());
    return virtreg_operand(vreg_for_port(p), p.type().element_width());
}

void x86_translation_context::materialise_write_reg(write_reg_node *n) {
    // Detect register decrement/increment : write X <= Read X (+/-) Constant
    if (n->value().owner()->kind() == node_kinds::binary_arith) {
        auto binop = (binary_arith_node *)n->value().owner();

        if (binop->lhs().owner()->kind() == node_kinds::read_reg &&
            binop->rhs().owner()->kind() == node_kinds::constant) {
            auto lhs = (read_reg_node *)binop->lhs().owner();
            auto rhs = (constant_node *)binop->rhs().owner();

            if (lhs->regoff() == n->regoff()) {
                switch (binop->op()) {
                case binary_arith_op::sub:
                    builder_.sub(
                        guestreg_memory_operand(
                            n->value().type().element_width(), n->regoff()),
                        x86_operand(x86_immediate_operand(rhs->const_val_i()),
                                    n->value().type().element_width()));
                    return;
                }
            }
        }
    }

    if (n->value().owner()->kind() == node_kinds::constant) {
        auto cv = (constant_node *)n->value().owner();
        builder_.mov(
            guestreg_memory_operand(n->value().type().element_width(),
                                    n->regoff()),
            imm_operand(cv->const_val_i(), n->value().type().element_width()));
    } else {
        builder_.mov(guestreg_memory_operand(n->value().type().element_width(),
                                             n->regoff()),
                     vreg_operand_for_port(n->value()));
    }
}

void x86_translation_context::materialise_read_reg(read_reg_node *n) {
    int value_vreg = alloc_vreg_for_port(n->val());
    int w = n->val().type().element_width() == 1
                ? 8
                : n->val().type().element_width();

    builder_.mov(virtreg_operand(value_vreg, w),
                 guestreg_memory_operand(w, n->regoff()));
}

void x86_translation_context::materialise_binary_arith(binary_arith_node *n) {
    int val_vreg = alloc_vreg_for_port(n->val());
    int z_vreg = alloc_vreg_for_port(n->zero());
    int n_vreg = alloc_vreg_for_port(n->negative());
    int v_vreg = alloc_vreg_for_port(n->overflow());
    int c_vreg = alloc_vreg_for_port(n->carry());

    int w = n->val().type().element_width();

    builder_.mov(virtreg_operand(val_vreg, w), vreg_operand_for_port(n->lhs()));

    switch (n->op()) {
    case binary_arith_op::bxor:
        builder_.xor_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n->rhs()));
        break;

    case binary_arith_op::band:
        builder_.and_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n->rhs()));
        break;

    case binary_arith_op::bor:
        builder_.or_(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n->rhs()));
        break;

    case binary_arith_op::add:
        builder_.add(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n->rhs()));
        break;

    case binary_arith_op::sub:
        builder_.sub(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n->rhs()));
        break;

    case binary_arith_op::mul:
        builder_.mul(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n->rhs(), false));
        break;

    default:
        throw std::runtime_error("unsupported binary arithmetic operation " +
                                 std::to_string((int)n->op()));
    }

    builder_.setz(virtreg_operand(z_vreg, 8));
    builder_.seto(virtreg_operand(v_vreg, 8));
    builder_.setc(virtreg_operand(c_vreg, 8));
    builder_.sets(virtreg_operand(n_vreg, 8));
}

void x86_translation_context::materialise_cast(cast_node *n) {
    int dst_vreg = alloc_vreg_for_port(n->val());

    switch (n->op()) {
    case cast_op::zx:
        builder_.movz(
            virtreg_operand(dst_vreg, n->val().type().element_width()),
            vreg_operand_for_port(n->source_value()));
        break;

    case cast_op::sx:
        builder_.movs(
            virtreg_operand(dst_vreg, n->val().type().element_width()),
            vreg_operand_for_port(n->source_value(), false));
        break;

    case cast_op::bitcast:
        builder_.mov(virtreg_operand(dst_vreg, n->val().type().element_width()),
                     vreg_operand_for_port(n->source_value()));
        break;

    default:
        throw std::runtime_error("unsupported cast operation");
    }
}

void x86_translation_context::materialise_constant(constant_node *n) {
    int dst_vreg = alloc_vreg_for_port(n->val());
    builder_.mov(
        virtreg_operand(dst_vreg, n->val().type().element_width()),
        imm_operand(n->const_val_i(), n->val().type().element_width()));
}

void x86_translation_context::materialise_read_pc(read_pc_node *n) {
    int dst_vreg = alloc_vreg_for_port(n->val());

    /*builder_.mov(virtreg_operand(dst_vreg, n->val().type().element_width()),
            x86_operand(x86_physical_register_operand(x86_register_names::R15),
       n->val().type().element_width()));*/

    builder_.mov(virtreg_operand(dst_vreg, n->val().type().element_width()),
                 x86_operand(x86_immediate_operand(this_pc_), 64));
}

void x86_translation_context::materialise_write_pc(write_pc_node *n) {
#if 0
	if (n->value().owner()->kind() == node_kinds::binary_arith) {
		auto binop = (binary_arith_node *)n->value().owner();
		if (binop->op() == binary_arith_op::add && binop->lhs().owner()->kind() == node_kinds::read_pc
			&& binop->rhs().owner()->kind() == node_kinds::constant) {
			auto imm = (constant_node *)binop->rhs().owner();

			builder_.mov(
				x86_operand(x86_physical_register_operand(x86_register_names::R15), 64), x86_operand(x86_immediate_operand(this_pc_ + imm->const_val_i()), 64));
			return;
		}
	}
#endif
    builder_.mov(
        x86_operand(x86_physical_register_operand(x86_register_names::R15),
                    n->value().type().element_width()),
        vreg_operand_for_port(n->value()));
}

void x86_translation_context::materialise_read_mem(read_mem_node *n) {
    int value_vreg = alloc_vreg_for_port(n->val());
    int w = n->val().type().element_width();

    materialise(n->address().owner());
    int addr_vreg = vreg_for_port(n->address());

    builder_.mov(
        virtreg_operand(value_vreg, w),
        x86_operand(x86_memory_operand(addr_vreg, 0, x86_register_names::GS),
                    w));
}

void x86_translation_context::materialise_write_mem(write_mem_node *n) {
    int w = n->value().type().element_width();

    materialise(n->address().owner());
    int addr_vreg = vreg_for_port(n->address());

    builder_.mov(
        x86_operand(x86_memory_operand(addr_vreg, 0, x86_register_names::GS),
                    w),
        vreg_operand_for_port(n->value()));
}

void x86_translation_context::do_register_allocation() { builder_.allocate(); }
