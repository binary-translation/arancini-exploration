
#include <arancini/ir/default-visitor.h>

#include <bitset>

#pragma once

class register_used_visitor : public arancini::ir::default_visitor {
public:
	void visit_read_reg_node(read_reg_node &node) override
	{
		default_visitor::visit_read_reg_node(node);
		if (node.regidx() >= 1 && node.regidx() <= 16) { // FIXME hardcoded
			reg_used_[node.regidx() - 1] = true;
		}
	}
	std::bitset<16> reg_used_;
};