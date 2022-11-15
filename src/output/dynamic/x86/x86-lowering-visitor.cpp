#include <arancini/output/dynamic/x86/machine-code-builder.h>
#include <arancini/output/dynamic/x86/x86-lowering-visitor.h>
#include <iostream>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::x86;
using namespace arancini::ir;

void x86_lowering_visitor::visit_label_node(label_node &n) { default_visitor::visit_label_node(n); }
void x86_lowering_visitor::visit_cond_br_node(cond_br_node &n) { default_visitor::visit_cond_br_node(n); }
void x86_lowering_visitor::visit_read_pc_node(read_pc_node &n) { default_visitor::visit_read_pc_node(n); }
void x86_lowering_visitor::visit_write_pc_node(write_pc_node &n) { default_visitor::visit_write_pc_node(n); }
void x86_lowering_visitor::visit_constant_node(constant_node &n) { default_visitor::visit_constant_node(n); }

void x86_lowering_visitor::visit_read_reg_node(read_reg_node &n)
{
	default_visitor::visit_read_reg_node(n);

	int v = alloc_vreg(&n);
	builder_.add_mov(operand::mem(regref::preg(physreg::rbp), n.regoff()), operand::vreg(v));
}

void x86_lowering_visitor::visit_read_mem_node(read_mem_node &n) { default_visitor::visit_read_mem_node(n); }

void x86_lowering_visitor::visit_write_reg_node(write_reg_node &n)
{
	default_visitor::visit_write_reg_node(n);
	builder_.add_mov(operand::vreg(get_vreg(n.value().owner())), operand::mem(regref::preg(physreg::rbp), n.regoff()));
}

void x86_lowering_visitor::visit_write_mem_node(write_mem_node &n) { default_visitor::visit_write_mem_node(n); }

void x86_lowering_visitor::visit_unary_arith_node(unary_arith_node &n) { default_visitor::visit_unary_arith_node(n); }

void x86_lowering_visitor::visit_binary_arith_node(binary_arith_node &n)
{
	default_visitor::visit_binary_arith_node(n);

	int dst = alloc_vreg(&n);

	builder_.add_mov(operand::vreg(get_vreg(n.rhs().owner())), operand::vreg(dst));

	opcodes op;
	switch (n.op()) {
	case binary_arith_op::bxor:
		op = opcodes::o_xor;
		break;
	case binary_arith_op::band:
		op = opcodes::o_and;
		break;
	case binary_arith_op::add:
		op = opcodes::add;
		break;
	case binary_arith_op::sub:
		op = opcodes::sub;
		break;
	}

	builder_.add_instruction(instruction(op, operand::vreg(get_vreg(n.lhs().owner())), operand::vreg(dst)));
}

void x86_lowering_visitor::visit_ternary_arith_node(ternary_arith_node &n) { default_visitor::visit_ternary_arith_node(n); }

void x86_lowering_visitor::visit_cast_node(cast_node &n)
{
	default_visitor::visit_cast_node(n);
	alloc_vreg(&n);
}

void x86_lowering_visitor::visit_csel_node(csel_node &n) { default_visitor::visit_csel_node(n); }
void x86_lowering_visitor::visit_bit_shift_node(bit_shift_node &n) { default_visitor::visit_bit_shift_node(n); }
void x86_lowering_visitor::visit_bit_extract_node(bit_extract_node &n) { default_visitor::visit_bit_extract_node(n); }
void x86_lowering_visitor::visit_bit_insert_node(bit_insert_node &n) { default_visitor::visit_bit_insert_node(n); }
void x86_lowering_visitor::visit_vector_extract_node(vector_extract_node &n) { default_visitor::visit_vector_extract_node(n); }
void x86_lowering_visitor::visit_vector_insert_node(vector_insert_node &n) { default_visitor::visit_vector_insert_node(n); }
