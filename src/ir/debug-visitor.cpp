#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

using namespace arancini::ir;

void debug_visitor::visit_chunk(chunk &c)
{
	chunk_name_ = "c" + std::to_string(chunk_idx_++);
	packet_idx_ = 0;

	apply_indent();

	os_ << "chunk " << chunk_name_ << std::endl;
	indent();

	default_visitor::visit_chunk(c);

	outdent();
}

void debug_visitor::visit_packet(packet &p)
{
	packet_name_ = chunk_name_ + "p" + std::to_string(packet_idx_++);
	node_idx_ = 0;

	apply_indent();

	os_ << "packet " << std::hex << &p << std::endl;
	indent();

	default_visitor::visit_packet(p);

	outdent();
}

void debug_visitor::visit_node(node &n) { node_names_[&n] = packet_name_ + "n" + std::to_string(node_idx_++); }

void debug_visitor::visit_label_node(label_node &n)
{
	default_visitor::visit_label_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "label" << std::endl;
}

void debug_visitor::visit_cond_br_node(cond_br_node &n)
{
	default_visitor::visit_cond_br_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "cond-br " << get_port_name(n.cond()) << std::endl;
}

void debug_visitor::visit_read_pc_node(read_pc_node &n)
{
	default_visitor::visit_read_pc_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "read-pc" << std::endl;
}

void debug_visitor::visit_write_pc_node(write_pc_node &n)
{
	default_visitor::visit_write_pc_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "write-pc" << std::endl;
}

void debug_visitor::visit_constant_node(constant_node &n)
{
	default_visitor::visit_constant_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "const #0x" << std::hex << n.const_val_i() << std::endl;
}

void debug_visitor::visit_read_reg_node(read_reg_node &n)
{
	default_visitor::visit_read_reg_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "read-reg #" << std::dec << n.regoff() << std::endl;
}

void debug_visitor::visit_read_mem_node(read_mem_node &n)
{
	default_visitor::visit_read_mem_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "read-mem" << std::endl;
}

void debug_visitor::visit_write_reg_node(write_reg_node &n)
{
	default_visitor::visit_write_reg_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "write-reg #" << std::dec << n.regoff() << ", " << get_port_name(n.value()) << std::endl;
}

void debug_visitor::visit_write_mem_node(write_mem_node &n)
{
	default_visitor::visit_write_mem_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "write-mem " << get_port_name(n.address()) << ", " << get_port_name(n.value()) << std::endl;
}

void debug_visitor::visit_unary_arith_node(unary_arith_node &n)
{
	default_visitor::visit_unary_arith_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "unop" << std::endl;
}

void debug_visitor::visit_binary_arith_node(binary_arith_node &n)
{
	default_visitor::visit_binary_arith_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";

	switch (n.op()) {
	case binary_arith_op::add:
		os_ << "add ";
		break;
	case binary_arith_op::band:
		os_ << "and ";
		break;
	case binary_arith_op::bor:
		os_ << "or ";
		break;
	case binary_arith_op::bxor:
		os_ << "xor ";
		break;
	case binary_arith_op::cmpeq:
		os_ << "cmp-eq ";
		break;
	case binary_arith_op::cmpne:
		os_ << "cmp-ne ";
		break;
	case binary_arith_op::div:
		os_ << "div ";
		break;
	case binary_arith_op::mul:
		os_ << "mul ";
		break;
	case binary_arith_op::sub:
		os_ << "sub ";
		break;
	}

	os_ << get_port_name(n.lhs()) << ", " << get_port_name(n.rhs()) << std::endl;
}

void debug_visitor::visit_ternary_arith_node(ternary_arith_node &n)
{
	default_visitor::visit_ternary_arith_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "ternop" << std::endl;
}

void debug_visitor::visit_cast_node(cast_node &n)
{
	default_visitor::visit_cast_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";

	switch (n.op()) {
	case cast_op::bitcast:
		os_ << "bitcast ";
		break;
	case cast_op::convert:
		os_ << "convert ";
		break;
	case cast_op::sx:
		os_ << "sx ";
		break;
	case cast_op::trunc:
		os_ << "trunc ";
		break;
	case cast_op::zx:
		os_ << "zx ";
		break;
	}

	os_ << get_port_name(n.source_value()) << " -> " << n.val().type().to_string() << std::endl;
}

void debug_visitor::visit_csel_node(csel_node &n)
{
	default_visitor::visit_csel_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "csel" << std::endl;
}

void debug_visitor::visit_bit_shift_node(bit_shift_node &n)
{
	default_visitor::visit_bit_shift_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "bit-shift" << std::endl;
}

void debug_visitor::visit_bit_extract_node(bit_extract_node &n)
{
	default_visitor::visit_bit_extract_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "bit-extract" << std::endl;
}

void debug_visitor::visit_bit_insert_node(bit_insert_node &n)
{
	default_visitor::visit_bit_insert_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "bit-insert" << std::endl;
}

void debug_visitor::visit_vector_extract_node(vector_extract_node &n)
{
	default_visitor::visit_vector_extract_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "vector-extract" << std::endl;
}

void debug_visitor::visit_vector_insert_node(vector_insert_node &n)
{
	default_visitor::visit_vector_insert_node(n);

	apply_indent();
	os_ << get_node_name(&n) << ": ";
	os_ << "vector-insert" << std::endl;
}
