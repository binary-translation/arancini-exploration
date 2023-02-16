#pragma once

namespace arancini::ir {
class chunk;
class packet;
class node;
class action_node;
class label_node;
class value_node;
class br_node;
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
class atomic_node;
class unary_atomic_node;
class binary_atomic_node;
class ternary_atomic_node;
class cast_node;
class csel_node;
class bit_shift_node;
class bit_extract_node;
class bit_insert_node;
class vector_node;
class vector_element_node;
class vector_extract_node;
class vector_insert_node;
class read_local_node;
class write_local_node;
class port;

class visitor {
public:
	virtual void visit_chunk(chunk &c) = 0;
	virtual void visit_packet(packet &p) = 0;

	// Nodes
	virtual void visit_node(node &n) = 0;
	virtual void visit_action_node(action_node &n) = 0;
	virtual void visit_label_node(label_node &n) = 0;
	virtual void visit_value_node(value_node &n) = 0;
	virtual void visit_br_node(br_node &n) = 0;
	virtual void visit_cond_br_node(cond_br_node &n) = 0;
	virtual void visit_read_pc_node(read_pc_node &n) = 0;
	virtual void visit_write_pc_node(write_pc_node &n) = 0;
	virtual void visit_constant_node(constant_node &n) = 0;
	virtual void visit_read_reg_node(read_reg_node &n) = 0;
	virtual void visit_read_mem_node(read_mem_node &n) = 0;
	virtual void visit_write_reg_node(write_reg_node &n) = 0;
	virtual void visit_write_mem_node(write_mem_node &n) = 0;
	virtual void visit_arith_node(arith_node &n) = 0;
	virtual void visit_unary_arith_node(unary_arith_node &n) = 0;
	virtual void visit_binary_arith_node(binary_arith_node &n) = 0;
	virtual void visit_ternary_arith_node(ternary_arith_node &n) = 0;
	virtual void visit_atomic_node(atomic_node &n) = 0;
	virtual void visit_unary_atomic_node(unary_atomic_node &n) = 0;
	virtual void visit_binary_atomic_node(binary_atomic_node &n) = 0;
	virtual void visit_ternary_atomic_node(ternary_atomic_node &n) = 0;
	virtual void visit_cast_node(cast_node &n) = 0;
	virtual void visit_csel_node(csel_node &n) = 0;
	virtual void visit_bit_shift_node(bit_shift_node &n) = 0;
	virtual void visit_bit_extract_node(bit_extract_node &n) = 0;
	virtual void visit_bit_insert_node(bit_insert_node &n) = 0;
	virtual void visit_vector_node(vector_node &n) = 0;
	virtual void visit_vector_element_node(vector_element_node &n) = 0;
	virtual void visit_vector_extract_node(vector_extract_node &n) = 0;
	virtual void visit_vector_insert_node(vector_insert_node &n) = 0;
	virtual void visit_read_local_node(read_local_node &n) = 0;
	virtual void visit_write_local_node(write_local_node &n) = 0;

	virtual void visit_port(port &p) = 0;

	virtual bool seen_node(node *n) = 0;
};
} // namespace arancini::ir
