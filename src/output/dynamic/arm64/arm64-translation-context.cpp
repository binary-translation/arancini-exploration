#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

void arm64_translation_context::begin_block() { }

void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) {
	instruction_index_to_guest_[builder_.nr_instructions()] = address;

	this_pc_ = address;
	std::cerr << "  " << std::hex << address << ": " << disasm << std::endl;
}

void arm64_translation_context::end_instruction() { }

void arm64_translation_context::end_block() {
	do_register_allocation();

    // TODO: add operations to finish block
	// builder_.xor_(
	// 	x86_operand(x86_physical_register_operand(x86_register_names::AX), 32), x86_operand(x86_physical_register_operand(x86_register_names::AX), 32));
	// builder_.ret();

	builder_.dump(std::cerr);

	builder_.emit(writer());

    // TODO: add code to dump
}

void arm64_translation_context::lower(ir::node *n) {
    materialise(n);
}

static arm64_operand virtreg_operand(unsigned int index, int width) {
    return arm64_operand(arm64_vreg_op(index), width == 1 ? 8 : width);
}

static arm64_operand imm_operand(unsigned long value, int width) {
    return arm64_operand(arm64_immediate_operand(value, width == 1 ? 8 : width));
}

static arm64_operand guestreg_memory_operand(int width, int regoff) {
	return arm64_operand(arm64_memory_operand(arm64_physreg_op::xzr_sp, regoff), width == 1 ? 8 : width);
}

arm64_operand arm64_translation_context::vreg_operand_for_port(port &p, bool constant_fold) {
    // TODO
	if (0 && constant_fold) {
		if (p.owner()->kind() == node_kinds::read_pc) {
			return arm64_operand(arm64_immediate_operand(this_pc_, 64));
		} else if (p.owner()->kind() == node_kinds::constant) {
			return arm64_operand(arm64_immediate_operand(((constant_node *)p.owner())->const_val_i(), p.type().width()));
		}
	}

	materialise(p.owner());
	return virtreg_operand(vreg_for_port(p), p.type().element_width());
}

void arm64_translation_context::materialise(const ir::node* n) {
    if (!n)
        throw std::runtime_error("ARM64 DBT received NULL pointer to node");

    switch (n->kind()) {
    case node_kinds::read_reg:
        return materialise_read_reg(reinterpret_cast<const read_reg_node&>(*n));
    case node_kinds::write_reg:
        return materialise_write_reg(reinterpret_cast<const write_reg_node&>(*n));
    case node_kinds::read_mem:
        return materialise_read_mem(reinterpret_cast<const read_mem_node&>(*n));
    case node_kinds::write_mem:
        return materialise_write_mem(reinterpret_cast<const write_mem_node&>(*n));
    case node_kinds::constant:
        return materialise_constant(reinterpret_cast<const constant_node&>(*n));
	case node_kinds::binary_arith:
		return materialise_binary_arith(reinterpret_cast<const binary_arith_node&>(*n));
    default:
        throw std::runtime_error("unknown node encountered");
    }
}

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
	int value_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width() == 1 ? 8 : n.val().type().element_width();

	builder_.mov(virtreg_operand(value_vreg, w), guestreg_memory_operand(w, n.regoff()));
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
	// Detect register decrement/increment : write X <= Read X (+/-) Constant
	if (n.value().owner()->kind() == node_kinds::binary_arith) {
		auto binop = (binary_arith_node *)n.value().owner();

		if (binop->lhs().owner()->kind() == node_kinds::read_reg && binop->rhs().owner()->kind() == node_kinds::constant) {
			auto lhs = (read_reg_node *)binop->lhs().owner();
			auto rhs = (constant_node *)binop->rhs().owner();

			if (lhs->regoff() == n.regoff()) {
				switch (binop->op()) {
				case binary_arith_op::sub:
					builder_.sub(guestreg_memory_operand(n.value().type().element_width(), n.regoff()),
						arm64_operand(arm64_immediate_operand(rhs->const_val_i(), n.value().type().element_width())));
					return;
                default:
                    throw std::runtime_error("unexpected");
				}
			}
		}
	}

	if (n.value().owner()->kind() == node_kinds::constant) {
		auto cv = (constant_node *)n.value().owner();
		builder_.mov(
			guestreg_memory_operand(n.value().type().element_width(), n.regoff()), imm_operand(cv->const_val_i(), n.value().type().element_width()));
	} else {
		builder_.mov(guestreg_memory_operand(n.value().type().element_width(), n.regoff()), vreg_operand_for_port(n.value()));
	}
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
	int value_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width();

	materialise(n.address().owner());
	int addr_vreg = vreg_for_port(n.address());

	builder_.mov(virtreg_operand(value_vreg, w), arm64_operand(arm64_memory_operand(addr_vreg, 0), w));
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
	int w = n.value().type().element_width();

	materialise(n.address().owner());
	int addr_vreg = vreg_for_port(n.address());

	builder_.mov(arm64_operand(arm64_memory_operand(addr_vreg, 0), w), vreg_operand_for_port(n.value()));
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	int dst_vreg = alloc_vreg_for_port(n.val());
	builder_.mov(virtreg_operand(dst_vreg, n.val().type().element_width()), imm_operand(n.const_val_i(), n.val().type().element_width()));
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
	int val_vreg = alloc_vreg_for_port(n.val());
	int z_vreg = alloc_vreg_for_port(n.zero());
	int n_vreg = alloc_vreg_for_port(n.negative());
	int v_vreg = alloc_vreg_for_port(n.overflow());
	int c_vreg = alloc_vreg_for_port(n.carry());

	int w = n.val().type().element_width();

	builder_.mov(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.lhs()));

	switch (n.op()) {
	case binary_arith_op::add:
		builder_.add(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::sub:
		builder_.sub(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::bxor:
		builder_.xor_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::band:
		builder_.and_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	default:
		throw std::runtime_error("unsupported binary arithmetic operation " + std::to_string((int)n.op()));
	}

	builder_.setz(virtreg_operand(z_vreg, 8));
	builder_.seto(virtreg_operand(v_vreg, 8));
	builder_.setc(virtreg_operand(c_vreg, 8));
	builder_.sets(virtreg_operand(n_vreg, 8));
}

void arm64_translation_context::do_register_allocation() { builder_.allocate(); }

