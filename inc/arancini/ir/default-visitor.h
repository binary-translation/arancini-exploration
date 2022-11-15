#pragma once

#include <arancini/ir/chunk.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
class default_visitor : public visitor {
public:
	virtual bool visit_chunk_start(chunk &c) { return true; }

	virtual bool visit_chunk_end(chunk &c) { return true; }

	virtual bool visit_packet_start(packet &p) { return true; }
	virtual bool visit_packet_end(packet &p) { return true; }

	// Nodes
	virtual bool visit_node(node &n) { return true; }
	virtual bool visit_action_node(action_node &n) { return true; }
	virtual bool visit_label_node(label_node &n) { return true; }
	virtual bool visit_value_node(value_node &n) { return true; }
	virtual bool visit_cond_br_node(cond_br_node &n) { return true; }
	virtual bool visit_read_pc_node(read_pc_node &n) { return true; }
	virtual bool visit_write_pc_node(write_pc_node &n) { return true; }
	virtual bool visit_constant_node(constant_node &n) { return true; }
	virtual bool visit_read_reg_node(read_reg_node &n) { return true; }
	virtual bool visit_read_mem_node(read_mem_node &n) { return true; }
	virtual bool visit_write_reg_node(write_reg_node &n) { return true; }
	virtual bool visit_write_mem_node(write_mem_node &n) { return true; }
	virtual bool visit_arith_node(arith_node &n) { return true; }
	virtual bool visit_unary_arith_node(unary_arith_node &n) { return true; }
	virtual bool visit_binary_arith_node(binary_arith_node &n) { return true; }
	virtual bool visit_ternary_arith_node(ternary_arith_node &n) { return true; }
	virtual bool visit_cast_node(cast_node &n) { return true; }
	virtual bool visit_csel_node(csel_node &n) { return true; }
	virtual bool visit_bit_shift_node(bit_shift_node &n) { return true; }
	virtual bool visit_bit_extract_node(bit_extract_node &n) { return true; }
	virtual bool visit_bit_insert_node(bit_insert_node &n) { return true; }
	virtual bool visit_vector_node(vector_node &n) { return true; }
	virtual bool visit_vector_element_node(vector_element_node &n) { return true; }
	virtual bool visit_vector_extract_node(vector_extract_node &n) { return true; }
	virtual bool visit_vector_insert_node(vector_insert_node &n) { return true; }

	virtual bool visit_port(port &p) { return true; }
};
} // namespace arancini::ir
