#pragma once

namespace arancini::ir {
class chunk;
class packet;
class node;
class action_node;
class value_node;
class start_node;
class end_node;
class read_pc_node;
class write_pc_node;
class constant_node;
class read_reg_node;
class read_mem_node;
class write_reg_node;
class write_mem_node;
class binary_arith_node;
class port;

class visitor {
public:
	virtual bool visit_chunk_start(chunk &c) = 0;
	virtual bool visit_chunk_end(chunk &c) = 0;

	virtual bool visit_packet(packet &p) = 0;

	// Nodes
	virtual bool visit_node(node &n) = 0;
	virtual bool visit_action_node(action_node &n) = 0;
	virtual bool visit_value_node(value_node &n) = 0;
	virtual bool visit_start_node(start_node &n) = 0;
	virtual bool visit_end_node(end_node &n) = 0;
	virtual bool visit_read_pc_node(read_pc_node &n) = 0;
	virtual bool visit_write_pc_node(write_pc_node &n) = 0;
	virtual bool visit_constant_node(constant_node &n) = 0;
	virtual bool visit_read_reg_node(read_reg_node &n) = 0;
	virtual bool visit_read_mem_node(read_mem_node &n) = 0;
	virtual bool visit_write_reg_node(write_reg_node &n) = 0;
	virtual bool visit_write_mem_node(write_mem_node &n) = 0;
	virtual bool visit_binary_arith_node(binary_arith_node &n) = 0;

	virtual bool visit_port(port &p) = 0;
};
} // namespace arancini::ir
