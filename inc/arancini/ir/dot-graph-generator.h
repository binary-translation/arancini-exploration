#pragma once

#include <arancini/ir/default-visitor.h>
#include <iostream>
#include <set>

namespace arancini::ir {
class dot_graph_generator : public default_visitor {
public:
	dot_graph_generator(std::ostream &os)
		: os_(os)
		, current_packet_(nullptr)
		, last_action_(nullptr)
		, cur_node_(nullptr)
	{
	}

	virtual void visit_chunk(chunk &c) override;
	virtual void visit_packet(packet &p) override;
	virtual void visit_node(node &n) override;
	virtual void visit_action_node(action_node &n) override;
	virtual void visit_label_node(label_node &n) override;
	virtual void visit_br_node(br_node &n) override;
	virtual void visit_cond_br_node(cond_br_node &n) override;
	virtual void visit_read_pc_node(read_pc_node &n) override;
	virtual void visit_write_pc_node(write_pc_node &n) override;
	virtual void visit_constant_node(constant_node &n) override;
	virtual void visit_read_reg_node(read_reg_node &n) override;
	virtual void visit_read_mem_node(read_mem_node &n) override;
	virtual void visit_write_reg_node(write_reg_node &n) override;
	virtual void visit_write_mem_node(write_mem_node &n) override;
	virtual void visit_unary_arith_node(unary_arith_node &n) override;
	virtual void visit_binary_arith_node(binary_arith_node &n) override;
	virtual void visit_ternary_arith_node(ternary_arith_node &n) override;
	virtual void visit_cast_node(cast_node &n) override;
	virtual void visit_csel_node(csel_node &n) override;
	virtual void visit_bit_shift_node(bit_shift_node &n) override;
	virtual void visit_bit_extract_node(bit_extract_node &n) override;
	virtual void visit_bit_insert_node(bit_insert_node &n) override;
	virtual void visit_vector_extract_node(vector_extract_node &n) override;
	virtual void visit_vector_insert_node(vector_insert_node &n) override;

private:
	std::ostream &os_;
	packet *current_packet_;
	action_node *last_action_;
	node *cur_node_;
	std::set<node *> seen_;

	void add_node(const node *n, const std::string &label) { os_ << std::hex << "N" << n << " [shape=Mrecord, label=\"" << label << "\"]" << std::endl; }

	std::string compute_port_label(const port *p) const
	{
		std::stringstream s;

		switch (p->kind()) {
		case port_kinds::value:
			s << "value";
			break;
		case port_kinds::constant:
			s << "#";
			break;
		case port_kinds::negative:
			s << "N";
			break;
		case port_kinds::overflow:
			s << "V";
			break;
		case port_kinds::carry:
			s << "C";
			break;
		case port_kinds::zero:
			s << "Z";
			break;
		default:
			s << "?";
			break;
		}

		s << ":" << p->type().to_string();
		return s.str();
	}

	void add_port_edge(const port *from, const node *to, const std::string &link = "") { add_edge(from->owner(), to, "black", compute_port_label(from), link); }

	void add_control_edge(const node *from, const node *to, const std::string &link = "") { add_edge(from, to, "green3", "", link); }

	void add_edge(const node *from, const node *to, const std::string &colour = "black", const std::string &label = "", const std::string &link = "")
	{
		os_ << std::hex << "N" << from << " -> "
			<< "N" << to;

		if (link != "") {
			os_ << ":" << link;
		}

		os_ << " [color=" << colour;

		if (label != "") {
			os_ << ", label=\"" << label << "\"";
		}

		os_ << "]" << std::endl;
	}
};
} // namespace arancini::ir
