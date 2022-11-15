#pragma once

#include <arancini/ir/default-visitor.h>
#include <ostream>

namespace arancini::ir {
class debug_visitor : public default_visitor {
public:
	debug_visitor(std::ostream &os)
		: os_(os)
		, level_(0)
	{
	}

	// virtual void visit_chunk(chunk &c) override;
	// virtual void visit_packet(packet &p) override;
	// virtual void visit_node(node &n) override;
	// virtual void visit_action_node(action_node &n) override;
	// virtual void visit_value_node(value_node &n) override;
	// virtual void visit_cond_br_node(cond_br_node &n) override;
	// virtual void visit_read_pc_node(read_pc_node &n) override;
	// virtual void visit_write_pc_node(write_pc_node &n) override;
	// virtual void visit_constant_node(constant_node &n) override;
	// virtual void visit_read_reg_node(read_reg_node &n) override;
	// virtual void visit_read_mem_node(read_mem_node &n) override;
	// virtual void visit_write_reg_node(write_reg_node &n) override;
	// virtual void visit_write_mem_node(write_mem_node &n) override;
	// virtual void visit_arith_node(arith_node &n) override;
	// virtual void visit_unary_arith_node(unary_arith_node &n) override;
	// virtual void visit_binary_arith_node(binary_arith_node &n) override;
	// virtual void visit_ternary_arith_node(ternary_arith_node &n) override;
	// virtual void visit_cast_node(cast_node &n) override;
	// virtual void visit_csel_node(csel_node &n) override;
	// virtual void visit_bit_shift_node(bit_shift_node &n) override;
	// virtual void visit_bit_extract_node(bit_extract_node &n) override;
	// virtual void visit_bit_insert_node(bit_insert_node &n) override;
	// virtual void visit_port(port &p) override;

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
