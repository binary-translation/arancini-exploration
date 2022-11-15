#pragma once

namespace arancini::ir {
class chunk;
class packet;
class node;
class action_node;
class label_node;
class value_node;
class cond_br_node;
class read_pc_node;
class write_pc_node;
class constant_node;
class read_reg_node;
class read_mem_node;
class write_reg_node;
class write_mem_node;
class arith_node;
class unary_arith_node;
class binary_arith_node;
class ternary_arith_node;
class cast_node;
class csel_node;
class bit_shift_node;
class bit_extract_node;
class bit_insert_node;
class vector_node;
class vector_element_node;
class vector_extract_node;
class vector_insert_node;
class port;

class visitor {
public:
	virtual bool visit_chunk_start(chunk &c) = 0;
	virtual bool visit_chunk_end(chunk &c) = 0;

	virtual bool visit_packet_start(packet &p) = 0;
	virtual bool visit_packet_end(packet &p) = 0;

	// Nodes
	virtual bool visit_node(node &n) = 0;
	virtual bool visit_action_node(action_node &n) = 0;
	virtual bool visit_label_node(label_node &n) = 0;
	virtual bool visit_value_node(value_node &n) = 0;
	virtual bool visit_cond_br_node(cond_br_node &n) = 0;
	virtual bool visit_read_pc_node(read_pc_node &n) = 0;
	virtual bool visit_write_pc_node(write_pc_node &n) = 0;
	virtual bool visit_constant_node(constant_node &n) = 0;
	virtual bool visit_read_reg_node(read_reg_node &n) = 0;
	virtual bool visit_read_mem_node(read_mem_node &n) = 0;
	virtual bool visit_write_reg_node(write_reg_node &n) = 0;
	virtual bool visit_write_mem_node(write_mem_node &n) = 0;
	virtual bool visit_arith_node(arith_node &n) = 0;
	virtual bool visit_unary_arith_node(unary_arith_node &n) = 0;
	virtual bool visit_binary_arith_node(binary_arith_node &n) = 0;
	virtual bool visit_ternary_arith_node(ternary_arith_node &n) = 0;
	virtual bool visit_cast_node(cast_node &n) = 0;
	virtual bool visit_csel_node(csel_node &n) = 0;
	virtual bool visit_bit_shift_node(bit_shift_node &n) = 0;
	virtual bool visit_bit_extract_node(bit_extract_node &n) = 0;
	virtual bool visit_bit_insert_node(bit_insert_node &n) = 0;
	virtual bool visit_vector_node(vector_node &n) = 0;
	virtual bool visit_vector_element_node(vector_element_node &n) = 0;
	virtual bool visit_vector_extract_node(vector_extract_node &n) = 0;
	virtual bool visit_vector_insert_node(vector_insert_node &n) = 0;

	virtual bool visit_port(port &p) = 0;
};
} // namespace arancini::ir
