#pragma once

#include <arancini/ir/visitor.h>
#include <ostream>

namespace arancini::ir {
class debug_visitor : public visitor {
public:
	debug_visitor(std::ostream &os)
		: os_(os)
		, level_(0)
	{
	}

	virtual bool visit_chunk_start(chunk &c) override;
	virtual bool visit_chunk_end(chunk &c) override;
	virtual bool visit_packet_start(packet &p) override;
	virtual bool visit_packet_end(packet &p) override;
	virtual bool visit_node(node &n) override;
	virtual bool visit_action_node(action_node &n) override;
	virtual bool visit_value_node(value_node &n) override;
	virtual bool visit_cond_br_node(cond_br_node &n) override;
	virtual bool visit_read_pc_node(read_pc_node &n) override;
	virtual bool visit_write_pc_node(write_pc_node &n) override;
	virtual bool visit_constant_node(constant_node &n) override;
	virtual bool visit_read_reg_node(read_reg_node &n) override;
	virtual bool visit_read_mem_node(read_mem_node &n) override;
	virtual bool visit_write_reg_node(write_reg_node &n) override;
	virtual bool visit_write_mem_node(write_mem_node &n) override;
	virtual bool visit_arith_node(arith_node &n) override;
	virtual bool visit_unary_arith_node(unary_arith_node &n) override;
	virtual bool visit_binary_arith_node(binary_arith_node &n) override;
	virtual bool visit_ternary_arith_node(ternary_arith_node &n) override;
	virtual bool visit_cast_node(cast_node &n) override;
	virtual bool visit_csel_node(csel_node &n) override;
	virtual bool visit_bit_shift_node(bit_shift_node &n) override;
	virtual bool visit_bit_extract_node(bit_extract_node &n) override;
	virtual bool visit_bit_insert_node(bit_insert_node &n) override;
	virtual bool visit_port(port &p) override;

private:
	std::ostream &os_;
	int level_;

	void indent() { level_++; }
	void outdent() { level_--; }

	void apply_indent()
	{
		for (int i = 0; i < level_; i++) {
			os_ << "  ";
		}
	}
};
} // namespace arancini::ir
