#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/output/dynamic/x86/x86-translation-context.h>
#include <iostream>

using namespace arancini::output::dynamic::x86;
using namespace arancini::ir;

void x86_translation_context::begin_block() { std::cerr << "begin block" << std::endl; }

void x86_translation_context::begin_instruction() { std::cerr << "begin insn" << std::endl; }

void x86_translation_context::end_instruction() { std::cerr << "end insn" << std::endl; }

void x86_translation_context::end_block()
{
	std::cerr << "end block" << std::endl;
	builder_.finalise(writer());
}

void x86_translation_context::lower(node *n)
{
	std::cerr << "lower node" << std::endl;

	debug_visitor v(std::cerr);
	n->accept(v);

	materialise(n);
}

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

	default:
		throw std::runtime_error("unsupported node");
	}

	materialised_nodes_.insert(n);
}

operand x86_translation_context::vreg_operand_for_port(port &p)
{
	materialise(p.owner());
	return operand::vreg(p.type().element_width(), port_to_vreg_.at(&p));
}

operand x86_translation_context::operand_for_port(port &p)
{
	if (p.owner()->kind() == node_kinds::constant) {
		return operand::imm(p.type().element_width(), ((constant_node *)p.owner())->const_val_i());
	}

	return vreg_operand_for_port(p);
}

static operand guestreg_operand(int width, int regoff) { return operand::mem(width, regref::preg(physreg::rbp), regoff); }

void x86_translation_context::materialise_write_reg(write_reg_node *n)
{
	builder_.add_mov(operand_for_port(n->value()), guestreg_operand(n->value().type().element_width(), n->regoff()));
}

void x86_translation_context::materialise_read_reg(read_reg_node *n)
{
	int value_vreg = alloc_vreg_for_port(n->val());
	int w = n->val().type().element_width();

	builder_.add_mov(guestreg_operand(w, n->regoff()), operand::vreg(w, value_vreg));
}

void x86_translation_context::materialise_binary_arith(binary_arith_node *n)
{
	int val_vreg = alloc_vreg_for_port(n->val());
	int z_vreg = alloc_vreg_for_port(n->zero());
	int n_vreg = alloc_vreg_for_port(n->negative());
	int v_vreg = alloc_vreg_for_port(n->overflow());
	int c_vreg = alloc_vreg_for_port(n->carry());

	int w = n->val().type().element_width();

	builder_.add_mov(vreg_operand_for_port(n->rhs()), operand::vreg(w, val_vreg));

	switch (n->op()) {
	case binary_arith_op::bxor:
		builder_.add_xor(vreg_operand_for_port(n->lhs()), operand::vreg(w, val_vreg));
		break;

	default:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	builder_.add_setz(operand::vreg(8, z_vreg));
	builder_.add_seto(operand::vreg(8, v_vreg));
	builder_.add_setc(operand::vreg(8, c_vreg));
	builder_.add_sets(operand::vreg(8, n_vreg));
}

void x86_translation_context::materialise_cast(cast_node *n)
{
	int dst_vreg = alloc_vreg_for_port(n->val());

	switch (n->op()) {
	case cast_op::zx:
		builder_.add_movz(vreg_operand_for_port(n->source_value()), operand::vreg(n->val().type().element_width(), dst_vreg));
        break;

	default:
		throw std::runtime_error("unsupported cast operation");
	}
}

void x86_translation_context::materialise_constant(constant_node *n)
{
	int dst_vreg = alloc_vreg_for_port(n->val());
	builder_.add_mov(operand::imm(n->val().type().element_width(), n->const_val_i()), operand::vreg(n->val().type().element_width(), dst_vreg));
}
