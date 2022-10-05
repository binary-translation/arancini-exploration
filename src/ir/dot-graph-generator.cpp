#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

using namespace arancini::ir;

bool dot_graph_generator::visit_chunk_start(chunk &c)
{
	os_ << "digraph chunk {" << std::endl;
	return true;
}

bool dot_graph_generator::visit_chunk_end(chunk &c)
{
	os_ << "}" << std::endl;
	return true;
}

bool dot_graph_generator::visit_packet_start(packet &p)
{
	char buffer[64];

	xed_format_context(XED_SYNTAX_INTEL, p.src_inst(), buffer, sizeof(buffer), p.address(), nullptr, 0);
	std::cerr << "[DEBUG] xed_format_context: " << buffer << std::endl;
	os_ << "subgraph cluster_" << std::hex << &p << " {" << std::endl;
	os_ << "label = \"" << buffer << "\";" << std::endl;

	if (last_packet_ && !last_packet_->actions().empty() && !p.actions().empty()) {
		os_ << "N" << last_packet_->actions().back() << " -> N" << p.actions().front() << " [color=blue];" << std::endl;
	}

	last_packet_ = &p;
	last_action_ = nullptr;

	return true;
}

bool dot_graph_generator::visit_packet_end(packet &p)
{
	os_ << "}" << std::endl;
	return true;
}

bool dot_graph_generator::visit_node(node &n)
{
	cur_node_ = &n;

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

bool dot_graph_generator::visit_cond_br_node(cond_br_node &n)
{
	os_ << "N" << &n << " [label=\"cond-br\"];" << std::endl;
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
	return true;
}

bool dot_graph_generator::visit_write_mem_node(write_mem_node &n)
{
	os_ << "N" << &n << " [label=\"write-mem\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_cast_node(cast_node &n)
{
	os_ << "N" << &n << " [label=\"";
	switch (n.op()) {
	case cast_op::sx:
		os_ << "sx";
		break;
	case cast_op::zx:
		os_ << "zx";
		break;
	case cast_op::trunc:
		os_ << "trunc";
		break;
	case cast_op::bitcast:
		os_ << "bitcast";
		break;
	}
	os_ << "\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_csel_node(csel_node &n)
{
	os_ << "N" << &n << " [label=\"csel\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_bit_shift_node(bit_shift_node &n)
{
	os_ << "N" << &n << " [label=\"";

	switch (n.op()) {
	case shift_op::asr:
		os_ << "asr";
		break;
	case shift_op::lsr:
		os_ << "lsr";
		break;
	case shift_op::lsl:
		os_ << "lsl";
		break;
	}

	os_ << "\"];" << std::endl;
	return true;
}

bool dot_graph_generator::visit_arith_node(arith_node &n) { return true; }

bool dot_graph_generator::visit_unary_arith_node(unary_arith_node &n)
{
	os_ << "N" << &n << " [label=\"";

	switch (n.op()) {
	case unary_arith_op::bnot:
		os_ << "not";
		break;
	case unary_arith_op::neg:
		os_ << "neg";
		break;
	case unary_arith_op::complement:
		os_ << "cmpl";
		break;
	default:
		os_ << "???";
		break;
	}

	os_ << "\"];" << std::endl;

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
	default:
		os_ << "???";
		break;
	}

	os_ << "\"];" << std::endl;

	return true;
}

bool dot_graph_generator::visit_ternary_arith_node(ternary_arith_node &n)
{
	os_ << "N" << &n << " [label=\"";

	switch (n.op()) {
	case ternary_arith_op::adc:
		os_ << "adc";
		break;
	case ternary_arith_op::sbb:
		os_ << "sbb";
		break;
	default:
		os_ << "???";
		break;
	}

	os_ << "\"];" << std::endl;

	return true;
}

bool dot_graph_generator::visit_port(port &p)
{
	std::string port_name = "?";
	switch (p.kind()) {
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
	case port_kinds::constant:
		port_name = "#";
		break;
	}

	for (auto target : p.targets()) {
		os_ << "N" << p.owner() << " -> N" << target << " [label=\"" << port_name << ":" << p.type().width() << "\"];" << std::endl;
	}

	return true;
}
