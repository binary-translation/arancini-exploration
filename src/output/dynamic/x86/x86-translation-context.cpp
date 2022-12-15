#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/output/dynamic/x86/x86-translation-context.h>
#include <iostream>

extern "C" {
#include <xed/xed-interface.h>
}

using namespace arancini::output::dynamic::x86;
using namespace arancini::ir;

void x86_translation_context::begin_block() { std::cerr << "INPUT ASSEMBLY:" << std::endl; }

void x86_translation_context::begin_instruction(off_t address, const std::string &disasm)
{
	std::cerr << "  " << std::hex << address << ": " << disasm << std::endl;
}

void x86_translation_context::end_instruction() { }

void x86_translation_context::end_block()
{
	do_register_allocation();
	builder_.emit(writer());

	std::cerr << "OUTPUT ASSEMBLY:" << std::endl;

	const unsigned char *code = (const unsigned char *)writer().ptr();
	size_t code_size = writer().size();

	off_t base_address = (off_t)code;
	size_t offset = 0;
	while (offset < code_size) {
		xed_decoded_inst_t xedd;
		xed_decoded_inst_zero(&xedd);
		xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
		xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_ALL);

		xed_error_enum_t xed_error = xed_decode(&xedd, &code[offset], code_size - offset);
		if (xed_error != XED_ERROR_NONE) {
			// throw std::runtime_error("unable to decode instruction: " + std::to_string(xed_error));
			break;
		}

		xed_uint_t length = xed_decoded_inst_get_length(&xedd);

		char buffer[64];
		xed_format_context(XED_SYNTAX_INTEL, &xedd, buffer, sizeof(buffer) - 1, base_address, nullptr, 0);
		std::cerr << "  " << std::hex << base_address << ": " << buffer << std::endl;

		offset += length;
		base_address += length;
	}
}

void x86_translation_context::lower(node *n) { materialise(n); }

void x86_translation_context::materialise(node *n)
{
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

static x86_memory guestreg_memory_operand(int width, int regoff) { return x86_memory(width, x86_register::rbp, regoff); }

x86_register x86_translation_context::vreg_operand_for_port(port &p)
{
	materialise(p.owner());
	return x86_register::virt(vreg_for_port(p), p.type().element_width());
}

void x86_translation_context::materialise_write_reg(write_reg_node *n)
{
	builder_.mov(guestreg_memory_operand(n->value().type().element_width(), n->regoff()), vreg_operand_for_port(n->value()));
}

void x86_translation_context::materialise_read_reg(read_reg_node *n)
{
	int value_vreg = alloc_vreg_for_port(n->val());
	int w = n->val().type().element_width();

	builder_.mov(x86_register::virt(w, value_vreg), guestreg_memory_operand(w, n->regoff()));
}

void x86_translation_context::materialise_binary_arith(binary_arith_node *n)
{
	int val_vreg = alloc_vreg_for_port(n->val());
	int z_vreg = alloc_vreg_for_port(n->zero());
	int n_vreg = alloc_vreg_for_port(n->negative());
	int v_vreg = alloc_vreg_for_port(n->overflow());
	int c_vreg = alloc_vreg_for_port(n->carry());

	int w = n->val().type().element_width();

	builder_.mov(x86_register::virt(w, val_vreg), vreg_operand_for_port(n->rhs()));

	switch (n->op()) {
	case binary_arith_op::bxor:
		builder_.xor_(x86_register::virt(w, val_vreg), vreg_operand_for_port(n->lhs()));
		break;

	case binary_arith_op::band:
		builder_.and_(x86_register::virt(w, val_vreg), vreg_operand_for_port(n->lhs()));
		break;

	case binary_arith_op::add:
		builder_.add(x86_register::virt(w, val_vreg), vreg_operand_for_port(n->lhs()));
		break;

	case binary_arith_op::sub:
		builder_.sub(x86_register::virt(w, val_vreg), vreg_operand_for_port(n->lhs()));
		break;

	default:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	builder_.setz(x86_register::virt(z_vreg, 1));
	builder_.seto(x86_register::virt(v_vreg, 1));
	builder_.setc(x86_register::virt(c_vreg, 1));
	builder_.sets(x86_register::virt(n_vreg, 1));
}

void x86_translation_context::materialise_cast(cast_node *n)
{
	int dst_vreg = alloc_vreg_for_port(n->val());

	switch (n->op()) {
	case cast_op::zx:
		builder_.movz(x86_register::virt(dst_vreg, n->val().type().element_width()), vreg_operand_for_port(n->source_value()));
		break;

	case cast_op::sx:
		builder_.movs(x86_register::virt(dst_vreg, n->val().type().element_width()), vreg_operand_for_port(n->source_value()));
		break;

	case cast_op::bitcast:
		builder_.mov(x86_register::virt(dst_vreg, n->val().type().element_width()), vreg_operand_for_port(n->source_value()));
		break;

	default:
		throw std::runtime_error("unsupported cast operation");
	}
}

void x86_translation_context::materialise_constant(constant_node *n)
{
	int dst_vreg = alloc_vreg_for_port(n->val());
	builder_.mov(x86_register::virt(dst_vreg, n->val().type().element_width()), x86_immediate(n->val().type().element_width(), n->const_val_i()));
}

void x86_translation_context::materialise_read_pc(read_pc_node *n)
{
	int dst_vreg = alloc_vreg_for_port(n->val());
	builder_.mov(x86_register::virt(dst_vreg, n->val().type().element_width()), x86_immediate(n->val().type().element_width(), 0x1234));
}

void x86_translation_context::materialise_write_pc(write_pc_node *n)
{
	// int dst_vreg = alloc_vreg_for_port(n->val());
	// builder_.add_mov(operand::imm(n->val().type().element_width(), 0x1234), operand::vreg(n->val().type().element_width(), dst_vreg));
}

void x86_translation_context::materialise_read_mem(read_mem_node *n)
{
	int value_vreg = alloc_vreg_for_port(n->val());
	int w = n->val().type().element_width();

	materialise(n->address().owner());
	int addr_vreg = vreg_for_port(n->address());

	// builder_.add_mov(operand::mem(w, segreg::fs, regref::vreg(addr_vreg), 0), operand::vreg(w, value_vreg));
	builder_.mov(x86_register::virt(w, value_vreg), x86_memory(w, x86_seg_reg::fs, x86_register::virt(addr_vreg, 64)));
}

void x86_translation_context::materialise_write_mem(write_mem_node *n)
{
	int w = n->value().type().element_width();

	materialise(n->address().owner());
	int addr_vreg = vreg_for_port(n->address());

	//	builder_.add_mov(vreg_operand_for_port(n->value()), operand::mem(w, segreg::fs, regref::vreg(addr_vreg), 0));

	builder_.mov(x86_memory(w, x86_seg_reg::fs, x86_register::virt(addr_vreg, 64)), vreg_operand_for_port(n->value()));
}

void x86_translation_context::do_register_allocation()
{
	//
}
