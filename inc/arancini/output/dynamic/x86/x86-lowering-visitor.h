#pragma once

#include <arancini/ir/default-visitor.h>
#include <map>

namespace arancini::output::dynamic {
class machine_code_writer;

namespace x86 {
	using namespace ir;

	class x86_lowering_visitor : public default_visitor {
	public:
		x86_lowering_visitor(machine_code_writer &writer)
			: writer_(writer)
			, next_vreg_(0)
		{
		}

		virtual void visit_label_node(label_node &n) override;
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
		machine_code_writer &writer_;
		std::map<node *, int> node_value_vreg_;
		int next_vreg_;

		int alloc_vreg(node *n)
		{
			int vr = next_vreg_++;
			node_value_vreg_[n] = vr;
			return vr;
		}
	};
} // namespace x86
} // namespace arancini::output::dynamic
