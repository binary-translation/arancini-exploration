#pragma once

#include <arancini/ir/chunk.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>
#include <set>

namespace arancini::ir {
class default_visitor : public visitor {
public:
	virtual void visit_chunk(chunk &c) override
	{
		for (auto p : c.packets()) {
			p->accept(*this);
		}
	}

	virtual void visit_packet(packet &p) override
	{
		for (auto n : p.actions()) {
			n->accept(*this);
		}
	}

	// Nodes
	virtual void visit_node(node &n) override { }

	virtual void visit_action_node(action_node &n) override { }

	virtual void visit_label_node(label_node &n) override { }

	virtual void visit_value_node(value_node &n) override { }

	virtual void visit_cond_br_node(cond_br_node &n) override
	{
		n.cond().accept(*this);
		n.target()->accept(*this);
	}

	virtual void visit_read_pc_node(read_pc_node &n) override { }

	virtual void visit_write_pc_node(write_pc_node &n) override { n.value().accept(*this); }

	virtual void visit_constant_node(constant_node &n) override { }

	virtual void visit_read_reg_node(read_reg_node &n) override { }

	virtual void visit_read_mem_node(read_mem_node &n) override { n.address().accept(*this); }

	virtual void visit_write_reg_node(write_reg_node &n) override { n.value().accept(*this); }

	virtual void visit_write_mem_node(write_mem_node &n) override
	{
		n.address().accept(*this);
		n.value().accept(*this);
	}

	virtual void visit_arith_node(arith_node &n) override { }

	virtual void visit_unary_arith_node(unary_arith_node &n) override { n.lhs().accept(*this); }

	virtual void visit_binary_arith_node(binary_arith_node &n) override
	{
		n.lhs().accept(*this);
		n.rhs().accept(*this);
	}

	virtual void visit_ternary_arith_node(ternary_arith_node &n) override
	{
		n.lhs().accept(*this);
		n.rhs().accept(*this);
		n.top().accept(*this);
	}

	virtual void visit_cast_node(cast_node &n) override { n.source_value().accept(*this); }

	virtual void visit_csel_node(csel_node &n) override
	{
		n.condition().accept(*this);
		n.trueval().accept(*this);
		n.falseval().accept(*this);
	}

	virtual void visit_bit_shift_node(bit_shift_node &n) override
	{
		n.input().accept(*this);
		n.amount().accept(*this);
	}

	virtual void visit_bit_extract_node(bit_extract_node &n) override { n.source_value().accept(*this); }

	virtual void visit_bit_insert_node(bit_insert_node &n) override
	{
		n.source_value().accept(*this);
		n.bits().accept(*this);
	}

	virtual void visit_vector_node(vector_node &n) override { }

	virtual void visit_vector_element_node(vector_element_node &n) override { }

	virtual void visit_vector_extract_node(vector_extract_node &n) override { n.source_vector().accept(*this); }

	virtual void visit_vector_insert_node(vector_insert_node &n) override
	{
		n.source_vector().accept(*this);
		n.insert_value().accept(*this);
	}

	virtual void visit_port(port &p) override
	{
		if (seen_.count(p.owner())) {
			return;
		}

		seen_.insert(p.owner());
		p.owner()->accept(*this);
	}

private:
	std::set<node *> seen_;
};
} // namespace arancini::ir
