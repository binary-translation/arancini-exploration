#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

using namespace arancini::ir;

bool debug_visitor::visit_chunk_start(chunk &c)
{
	apply_indent();
	os_ << "chunk start " << &c << std::endl;

	indent();
	return true;
}

bool debug_visitor::visit_chunk_end(chunk &c)
{
	outdent();

	apply_indent();
	os_ << "chunk end " << &c << std::endl;

	return true;
}

bool debug_visitor::visit_packet_start(packet &p)
{
	apply_indent();
	os_ << "packet start " << &p << std::endl;

	indent();
	return true;
}

bool debug_visitor::visit_packet_end(packet &p)
{
	outdent();

	apply_indent();
	os_ << "packent end " << &p << std::endl;

	return true;
}

bool debug_visitor::visit_node(node &n)
{
	apply_indent();
	os_ << "visit node " << &n << std::endl;
	return true;
}

bool debug_visitor::visit_action_node(action_node &n) { return true; }

bool debug_visitor::visit_value_node(value_node &n) { return true; }

bool debug_visitor::visit_cond_br_node(cond_br_node &n) { return true; }

bool debug_visitor::visit_read_pc_node(read_pc_node &n) { return true; }

bool debug_visitor::visit_write_pc_node(write_pc_node &n) { return true; }

bool debug_visitor::visit_constant_node(constant_node &n) { return true; }

bool debug_visitor::visit_read_reg_node(read_reg_node &n) { return true; }

bool debug_visitor::visit_read_mem_node(read_mem_node &n) { return true; }

bool debug_visitor::visit_write_reg_node(write_reg_node &n) { return true; }

bool debug_visitor::visit_write_mem_node(write_mem_node &n) { return true; }

bool debug_visitor::visit_arith_node(arith_node &n) { return true; }

bool debug_visitor::visit_unary_arith_node(unary_arith_node &n) { return true; }

bool debug_visitor::visit_binary_arith_node(binary_arith_node &n)
{
	indent();
	apply_indent();
	os_ << "visit binary airth: ";
	switch(n.op()) {
		case binary_arith_op::add:
		os_ << "add";
		break;
		case binary_arith_op::sub:
		os_ << "sub";
		break;
		case binary_arith_op::mul:
		os_ << "mul";
		break;
		case binary_arith_op::div:
		os_ << "div";
		break;
		case binary_arith_op::band:
		os_ << "band";
		break;
		case binary_arith_op::bor:
		os_ << "bor";
		break;
		case binary_arith_op::bxor:
		os_ << "bxor";
		break;
		case binary_arith_op::cmpeq:
		os_ << "cmpeq";
		break;
		case binary_arith_op::cmpne:
		os_ << "cmpne";
		break;
		default:
		os_ << "unknown op";
	}
	os_ << std::endl;

	outdent();
	return true;
}

bool debug_visitor::visit_ternary_arith_node(ternary_arith_node &n) { return true; }

bool debug_visitor::visit_cast_node(cast_node &n) { return true; }

bool debug_visitor::visit_csel_node(csel_node &n) { return true; }

bool debug_visitor::visit_bit_shift_node(bit_shift_node &n) { return true; }

bool debug_visitor::visit_bit_extract_node(bit_extract_node &n) { return true; }

bool debug_visitor::visit_bit_insert_node(bit_insert_node &n) { return true; }

bool debug_visitor::visit_port(port &p)
{
	indent();
	apply_indent();
	os_ << "visit port" << std::endl;

	indent();
	for (auto t : p.targets()) {
		apply_indent();
		os_ << "target " << t << std::endl;
	}
	outdent();

	outdent();
	return true;
}
