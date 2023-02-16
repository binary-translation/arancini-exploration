#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <cstdio>
#include <sstream>
#include <string>

using namespace arancini::ir;

void dot_graph_generator::visit_chunk(chunk &c)
{
	os_ << "digraph chunk {" << std::endl;
	default_visitor::visit_chunk(c);
	os_ << " }" << std::endl;
}

void dot_graph_generator::visit_packet(packet &p)
{
	os_ << "subgraph cluster_" << std::hex << &p << " {" << std::endl;
	os_ << "label = \"@0x" << std::hex << p.address() << ": " << p.disassembly() << "\";" << std::endl;

	if (current_packet_ && !current_packet_->actions().empty() && !p.actions().empty()) {
		add_edge(current_packet_->actions().back(), p.actions().front(), "blue2");
	}

	current_packet_ = &p;
	last_action_ = nullptr;

	default_visitor::visit_packet(p);

	os_ << "}" << std::endl;
}

void dot_graph_generator::visit_node(node &n)
{
	cur_node_ = &n;
	seen_.insert(&n);

	default_visitor::visit_node(n);
}

void dot_graph_generator::visit_action_node(action_node &n)
{
	// If there was a "last action", then connect the "last action" to this action node
	// with a control-flow edge.
	if (last_action_) {
		add_edge(last_action_, &n, "red2");
	}

	// Update the current "last action"
	last_action_ = &n;

	default_visitor::visit_action_node(n);
}

void dot_graph_generator::visit_label_node(label_node &n)
{
	std::stringstream s;
	s << "label [" << n.name() << "]";
	add_node(&n, s.str());
	default_visitor::visit_label_node(n);
}

void dot_graph_generator::visit_br_node(br_node &n)
{
	add_node(&n, "br");
	add_control_edge(&n, n.target());
	default_visitor::visit_br_node(n);
}

void dot_graph_generator::visit_cond_br_node(cond_br_node &n)
{
	add_node(&n, "cond-br");
	add_control_edge(&n, n.target());
	add_port_edge(&n.cond(), &n, "cond");
	default_visitor::visit_cond_br_node(n);
}

void dot_graph_generator::visit_read_pc_node(read_pc_node &n)
{
	add_node(&n, "read-pc");
	default_visitor::visit_read_pc_node(n);
}

void dot_graph_generator::visit_write_pc_node(write_pc_node &n)
{
	add_node(&n, "write-pc");
	add_port_edge(&n.value(), &n);
	default_visitor::visit_write_pc_node(n);
}

void dot_graph_generator::visit_constant_node(constant_node &n)
{
	std::stringstream s;
	s << "constant 0x" << std::hex << n.const_val_i();

	add_node(&n, s.str());
	default_visitor::visit_constant_node(n);
}

void dot_graph_generator::visit_read_reg_node(read_reg_node &n)
{
	std::stringstream s;
	s << "read-reg " << n.regname();

	add_node(&n, s.str());
	default_visitor::visit_read_reg_node(n);
}

void dot_graph_generator::visit_read_mem_node(read_mem_node &n)
{
	add_node(&n, "read-mem");
	add_port_edge(&n.address(), &n);
	default_visitor::visit_read_mem_node(n);
}

void dot_graph_generator::visit_write_reg_node(write_reg_node &n)
{
	std::stringstream s;
	s << "write-reg " << n.regname();

	add_node(&n, s.str());
	add_port_edge(&n.value(), &n);

	default_visitor::visit_write_reg_node(n);
}

void dot_graph_generator::visit_write_mem_node(write_mem_node &n)
{
	add_node(&n, "{{<addr>addr|<val>val}|write-mem}");
	add_port_edge(&n.address(), &n, "addr");
	add_port_edge(&n.value(), &n, "val");

	default_visitor::visit_write_mem_node(n);
}

void dot_graph_generator::visit_cast_node(cast_node &n)
{
	std::stringstream s;

	switch (n.op()) {
	case cast_op::sx:
		s << "sx";
		break;
	case cast_op::zx:
		s << "zx";
		break;
	case cast_op::trunc:
		s << "trunc";
		break;
	case cast_op::bitcast:
		s << "bitcast";
		break;
	case cast_op::convert:
		s << "convert";
		break;
	default:
		s << "unknown-cast";
		break;
	}

	add_node(&n, s.str());
	add_port_edge(&n.source_value(), &n);

	default_visitor::visit_cast_node(n);
}

void dot_graph_generator::visit_csel_node(csel_node &n)
{
	add_node(&n, "{{<cond>cond|<tv>true|<fv>false}|cond-sel}");

	add_port_edge(&n.condition(), &n, "cond");
	add_port_edge(&n.trueval(), &n, "tv");
	add_port_edge(&n.falseval(), &n, "fv");

	default_visitor::visit_csel_node(n);
}

void dot_graph_generator::visit_bit_shift_node(bit_shift_node &n)
{
	std::stringstream s;

	s << "{{<val>val|<amt>amount}|";

	switch (n.op()) {
	case shift_op::asr:
		s << "asr";
		break;
	case shift_op::lsr:
		s << "lsr";
		break;
	case shift_op::lsl:
		s << "lsl";
		break;
	default:
		s << "unknown-shift";
		break;
	}

	s << "}";

	add_node(&n, s.str());
	add_port_edge(&n.input(), &n, "val");
	add_port_edge(&n.amount(), &n, "amt");

	default_visitor::visit_bit_shift_node(n);
}

void dot_graph_generator::visit_unary_arith_node(unary_arith_node &n)
{
	std::stringstream s;

	switch (n.op()) {
	case unary_arith_op::bnot:
		s << "not";
		break;
	case unary_arith_op::neg:
		s << "neg";
		break;
	case unary_arith_op::complement:
		s << "cmpl";
		break;
	default:
		s << "unknown-unary";
		break;
	}

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n);

	default_visitor::visit_unary_arith_node(n);
}

void dot_graph_generator::visit_binary_arith_node(binary_arith_node &n)
{
	std::stringstream s;

	s << "{{<lhs>LHS|<rhs>RHS}|";

	switch (n.op()) {
	case binary_arith_op::add:
		s << "add";
		break;
	case binary_arith_op::sub:
		s << "sub";
		break;
	case binary_arith_op::mul:
		s << "mul";
		break;
	case binary_arith_op::div:
		s << "div";
		break;
	case binary_arith_op::mod:
		s << "mod";
		break;
	case binary_arith_op::band:
		s << "and";
		break;
	case binary_arith_op::bor:
		s << "or";
		break;
	case binary_arith_op::bxor:
		s << "xor";
		break;
	case binary_arith_op::cmpeq:
		s << "cmpeq";
		break;
	case binary_arith_op::cmpne:
		s << "cmpne";
		break;
	case binary_arith_op::cmpgt:
		s << "cmpgt";
		break;
	default:
		s << "unknown-binary";
		break;
	}

	s << "}";

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n, "lhs");
	add_port_edge(&n.rhs(), &n, "rhs");

	default_visitor::visit_binary_arith_node(n);
}

void dot_graph_generator::visit_ternary_arith_node(ternary_arith_node &n)
{
	std::stringstream s;

	s << "{{<lhs>LHS|<rhs>RHS|<top>TOP}|";

	switch (n.op()) {
	case ternary_arith_op::adc:
		s << "adc";
		break;
	case ternary_arith_op::sbb:
		s << "sbb";
		break;
	default:
		s << "unknown-ternary";
		break;
	}

	s << "}";

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n, "lhs");
	add_port_edge(&n.rhs(), &n, "rhs");
	add_port_edge(&n.top(), &n, "top");

	default_visitor::visit_ternary_arith_node(n);
}

void dot_graph_generator::visit_unary_atomic_node(unary_atomic_node &n)
{
	std::stringstream s;

	switch (n.op()) {
	case unary_atomic_op::bnot:
		s << "atomic not";
		break;
	case unary_atomic_op::neg:
		s << "atomic neg";
		break;
	default:
		s << "unknown-atomic-unary";
		break;
	}

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n);

	default_visitor::visit_unary_atomic_node(n);
}

void dot_graph_generator::visit_binary_atomic_node(binary_atomic_node &n)
{
	std::stringstream s;

	s << "{{<lhs>LHS|<rhs>RHS}|";

	switch (n.op()) {
	case binary_atomic_op::add:
		s << "atomic add";
		break;
	case binary_atomic_op::sub:
		s << "atomic sub";
		break;
	case binary_atomic_op::band:
		s << "atomic and";
		break;
	case binary_atomic_op::bor:
		s << "atomic bor";
		break;
	case binary_atomic_op::xadd:
		s << "atomic xadd";
		break;
	case binary_atomic_op::bxor:
		s << "atomic xor";
		break;
	case binary_atomic_op::btc:
		s << "atomic btc";
		break;
	case binary_atomic_op::btr:
		s << "atomic btr";
		break;
	case binary_atomic_op::bts:
		s << "atomic bts";
		break;
	case binary_atomic_op::xchg:
		s << "atomic xchg";
		break;
	default:
		s << "unknown-atomic-binary";
		break;
	}

	s << "}";

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n, "lhs");
	add_port_edge(&n.rhs(), &n, "rhs");

	default_visitor::visit_binary_atomic_node(n);
}

void dot_graph_generator::visit_ternary_atomic_node(ternary_atomic_node &n)
{
	std::stringstream s;

	s << "{{<lhs>LHS|<rhs>RHS|<top>TOP}|";

	switch (n.op()) {
	case ternary_atomic_op::adc:
		s << "atomic adc";
		break;
	case ternary_atomic_op::sbb:
		s << "atomic sbb";
		break;
	case ternary_atomic_op::cmpxchg:
		s << "atomic cmpxchg";
		break;
	default:
		s << "unknown-atomic-ternary";
		break;
	}

	s << "}";

	add_node(&n, s.str());
	add_port_edge(&n.lhs(), &n, "lhs");
	add_port_edge(&n.rhs(), &n, "rhs");
	add_port_edge(&n.top(), &n, "top");

	default_visitor::visit_ternary_atomic_node(n);
}

void dot_graph_generator::visit_bit_extract_node(bit_extract_node &n)
{
	std::stringstream s;

	s << "bit-extract [" << n.from() + n.length() - 1 << ":" << n.from() << "]";

	add_node(&n, s.str());
	add_port_edge(&n.source_value(), &n);

	default_visitor::visit_bit_extract_node(n);
}

void dot_graph_generator::visit_bit_insert_node(bit_insert_node &n)
{
	std::stringstream s;

	s << "{{<dst>DST|<bits>BITS}|";
	s << "bit-insert [" << n.to() + n.length() - 1 << ":" << n.to() << "]}";

	add_node(&n, s.str());
	add_port_edge(&n.source_value(), &n, "dst");
	add_port_edge(&n.bits(), &n, "bits");

	default_visitor::visit_bit_insert_node(n);
}

void dot_graph_generator::visit_vector_extract_node(vector_extract_node &n)
{
	std::stringstream s;

	s << "vector-extract [" << n.index() << "]";

	add_node(&n, s.str());
	add_port_edge(&n.source_vector(), &n);

	default_visitor::visit_vector_extract_node(n);
}

void dot_graph_generator::visit_vector_insert_node(vector_insert_node &n)
{
	std::stringstream s;

	s << "{{<dst>DST|<elt>ELT}|";
	s << "vector-insert [" << n.index() << "]}";

	add_node(&n, s.str());
	add_port_edge(&n.source_vector(), &n, "dst");
	add_port_edge(&n.insert_value(), &n, "elt");

	default_visitor::visit_vector_insert_node(n);
}

void dot_graph_generator::visit_read_local_node(read_local_node &n)
{
	std::stringstream s;

	s << "read-local " << std::hex << (uintptr_t)&n.local();

	add_node(&n, s.str());

	default_visitor::visit_read_local_node(n);
}

void dot_graph_generator::visit_write_local_node(write_local_node &n)
{
	std::stringstream s;

	s << "write-local " << std::hex << (uintptr_t)&n.local();

	add_node(&n, s.str());
	add_port_edge(&n.write_value(), &n);

	default_visitor::visit_write_local_node(n);
}
