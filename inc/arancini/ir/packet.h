#pragma once

#include <vector>

#include <arancini/ir/node.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
class packet {
public:
	constant_node *insert_constant(const value_type &vt, unsigned long cv) { return insert(new constant_node(*this, vt, cv)); }
	constant_node *insert_constant_u32(unsigned int cv) { return insert(new constant_node(*this, value_type::u32(), cv)); }
	constant_node *insert_constant_u64(unsigned long cv) { return insert(new constant_node(*this, value_type::u64(), cv)); }

	start_node *insert_start(unsigned long offset) { return insert(new start_node(*this, offset)); }
	end_node *insert_end() { return insert(new end_node(*this)); }

	read_pc_node *insert_read_pc() { return insert(new read_pc_node(*this)); }
	write_pc_node *insert_write_pc(port &value) { return insert(new write_pc_node(*this, value)); }

	read_reg_node *insert_read_reg(const value_type &vt, unsigned long regoff) { return insert(new read_reg_node(*this, vt, regoff)); }
	write_reg_node *insert_write_reg(unsigned long regoff, port &value) { return insert(new write_reg_node(*this, regoff, value)); }
	read_mem_node *insert_read_mem(const value_type &vt, port &addr) { return insert(new read_mem_node(*this, vt, addr)); }
	write_mem_node *insert_write_mem(port &addr, port &value) { return insert(new write_mem_node(*this, addr, value)); }

	binary_arith_node *insert_add(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::add, lhs, rhs)); }
	binary_arith_node *insert_sub(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::sub, lhs, rhs)); }
	binary_arith_node *insert_mul(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::mul, lhs, rhs)); }
	binary_arith_node *insert_div(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::div, lhs, rhs)); }
	binary_arith_node *insert_and(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::band, lhs, rhs)); }
	binary_arith_node *insert_or(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bor, lhs, rhs)); }
	binary_arith_node *insert_xor(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bxor, lhs, rhs)); }

	cast_node *insert_zx(const value_type &target_type, port &value)
	{
		/*if (target_type.width() == value.type().width()) {
			return val
		}*/

		return insert(new cast_node(*this, cast_op::zx, target_type, value));
	}

	const std::vector<action_node *> &actions() const { return actions_; }

	bool accept(visitor &v)
	{
		if (!v.visit_packet_start(*this)) {
			return false;
		}

		for (auto n : actions_) {
			n->accept(v);
		}

		return v.visit_packet_end(*this);
	}

	start_node *get_start_node() const { return (start_node *)actions_.front(); }

private:
	std::vector<action_node *> actions_;

	template <class T> T *insert(T *n)
	{
		if (n->is_action()) {
			actions_.push_back((action_node *)n);
		}

		return n;
	}
};
} // namespace arancini::ir
