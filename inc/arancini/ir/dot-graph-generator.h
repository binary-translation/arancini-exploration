#pragma once

#include <arancini/ir/visitor.h>
#include <iostream>
#include <set>

namespace arancini::ir {
class dot_graph_generator : public visitor {
public:
	dot_graph_generator(std::ostream &os)
		: os_(os)
		, last_packet_(nullptr)
		, last_action_(nullptr)
		, cur_node_(nullptr)
	{
	}

	virtual bool visit_chunk_start(chunk &c) override;
	virtual bool visit_chunk_end(chunk &c) override;
	virtual bool visit_packet_start(packet &p) override;
	virtual bool visit_packet_end(packet &p) override;
	virtual bool visit_node(node &n) override;
	virtual bool visit_action_node(action_node &n) override;
	virtual bool visit_value_node(value_node &n) override;
	virtual bool visit_start_node(start_node &n) override;
	virtual bool visit_end_node(end_node &n) override;
	virtual bool visit_read_pc_node(read_pc_node &n) override;
	virtual bool visit_write_pc_node(write_pc_node &n) override;
	virtual bool visit_constant_node(constant_node &n) override;
	virtual bool visit_read_reg_node(read_reg_node &n) override;
	virtual bool visit_read_mem_node(read_mem_node &n) override;
	virtual bool visit_write_reg_node(write_reg_node &n) override;
	virtual bool visit_write_mem_node(write_mem_node &n) override;
	virtual bool visit_binary_arith_node(binary_arith_node &n) override;
	virtual bool visit_cast_node(cast_node &n) override;
	virtual bool visit_port(port &p) override;

private:
	std::ostream &os_;
	packet *last_packet_;
	action_node *last_action_;
	node *cur_node_;
	std::set<node *> seen_;
};
} // namespace arancini::ir
