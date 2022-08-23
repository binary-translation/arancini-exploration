#include <arancini/ir/block.h>
#include <arancini/ir/builder.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

using namespace arancini::ir;

bool dot_graph_generator::visit_builder_start(builder &b)
{
	os_ << "digraph a {" << std::endl;
	return true;
}
bool dot_graph_generator::visit_builder_end(builder &b)
{
	os_ << "}" << std::endl;
	return true;
}
bool dot_graph_generator::visit_block(block &b) { return true; }

bool dot_graph_generator::visit_packet(packet &p)
{
	if (last_packet_ && !last_packet_->actions().empty() && !p.actions().empty()) {
		os_ << "N" << last_packet_->actions().back() << " -> N" << p.actions().front() << " [color=blue];" << std::endl;
	}

	last_packet_ = &p;
	last_action_ = nullptr;

	return true;
}

bool dot_graph_generator::visit_node(node &n)
{
	if (seen_.count(&n)) {
		return false;
	}
	seen_.insert(&n);
	return true;
}

bool dot_graph_generator::visit_action_node(action_node &n)
{
	if (last_action_) {
		os_ << "N" << last_action_ << " -> N" << &n << " [color=red];" << std::endl;
	}

	last_action_ = &n;

	return true;
}

bool dot_graph_generator::visit_value_node(value_node &n) { return true; }

bool dot_graph_generator::visit_start_node(start_node &n)
{
	os_ << "N" << &n << " [label=\"start #" << std::hex << n.offset() << "\"];" << std::endl;
	return true;
}
bool dot_graph_generator::visit_end_node(end_node &n)
{
	os_ << "N" << &n << " [label=\"end\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_read_pc_node(read_pc_node &n)
{
	os_ << "N" << &n << " [label=\"read-pc\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_write_pc_node(write_pc_node &n)
{
	os_ << "N" << &n << " [label=\"write-pc\"];" << std::endl;
	os_ << "N" << n.value().owner() << " -> N" << &n << ";" << std::endl;
	return true;
}

bool dot_graph_generator::visit_constant_node(constant_node &n)
{
	os_ << "N" << &n << " [label=\"constant #" << n.const_val() << "\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_read_reg_node(read_reg_node &n)
{
	os_ << "N" << &n << " [label=\"read-reg #" << n.regoff() << "\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_read_mem_node(read_mem_node &n)
{
	os_ << "N" << &n << " [label=\"read-mem\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_write_reg_node(write_reg_node &n)
{
	os_ << "N" << &n << " [label=\"write-reg #" << n.regoff() << "\"];" << std::endl;

	std::string port_name = "?";
	switch (n.value().kind()) {
	case port_kinds::value:
		port_name = "value";
		break;
	case port_kinds::zero:
		port_name = "Z";
		break;
	case port_kinds::negative:
		port_name = "N";
		break;
	case port_kinds::overflow:
		port_name = "V";
		break;
	case port_kinds::carry:
		port_name = "C";
		break;
	}

	os_ << "N" << n.value().owner() << " -> N" << &n << " [label=\"" << port_name << "\"];" << std::endl;

	return true;
}

bool dot_graph_generator::visit_write_mem_node(write_mem_node &n)
{
	os_ << "N" << &n << " [label=\"write-mem\"];" << std::endl;
	os_ << "N" << n.address().owner() << " -> N" << &n << ";" << std::endl;
	os_ << "N" << n.value().owner() << " -> N" << &n << ";" << std::endl;

	return true;
}

bool dot_graph_generator::visit_binary_arith_node(binary_arith_node &n)
{
	os_ << "N" << &n << " [label=\"";

	switch (n.op()) {
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
		os_ << "and";
		break;
	case binary_arith_op::bor:
		os_ << "or";
		break;
	case binary_arith_op::bxor:
		os_ << "xor";
		break;
	}

	os_ << "\"];" << std::endl;

	os_ << "N" << n.lhs().owner() << " -> N" << &n << ";" << std::endl;
	os_ << "N" << n.rhs().owner() << " -> N" << &n << ";" << std::endl;

	return true;
}

bool dot_graph_generator::visit_port(port &p) { return true; }
