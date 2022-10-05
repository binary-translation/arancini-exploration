#pragma once

#include <vector>

#include <arancini/ir/node.h>
#include <arancini/ir/visitor.h>

extern "C" {
#include <xed/xed-interface.h>
}

namespace arancini::ir {
class packet {
public:
	packet(off_t address, xed_decoded_inst_t *src_inst)
		: address_(address), src_inst_(src_inst)
	{
	}

	value_node *insert_constant_i(const value_type &vt, unsigned long cv) { return insert(new constant_node(*this, vt, cv)); }
	value_node *insert_constant_f(const value_type &vt, double cv) { return insert(new constant_node(*this, vt, cv)); }
	value_node *insert_constant_u8(unsigned char cv) { return insert(new constant_node(*this, value_type::u8(), (unsigned long)cv)); }
	value_node *insert_constant_u16(unsigned short cv) { return insert(new constant_node(*this, value_type::u16(), (unsigned long)cv)); }
	value_node *insert_constant_u32(unsigned int cv) { return insert(new constant_node(*this, value_type::u32(), (unsigned long)cv)); }
	value_node *insert_constant_s32(signed int cv) { return insert(new constant_node(*this, value_type::s32(), (unsigned long)cv)); }
	value_node *insert_constant_u64(unsigned long cv) { return insert(new constant_node(*this, value_type::u64(), (unsigned long)cv)); }
	value_node *insert_constant_f32(float cv) { return insert(new constant_node(*this, value_type::f32(), (double)cv)); }
	value_node *insert_constant_f64(double cv) { return insert(new constant_node(*this, value_type::f64(), (double)cv)); }

	label_node *insert_label() { return insert(new label_node(*this)); }
	action_node *insert_cond_br(port &cond, label_node *target) { return insert(new cond_br_node(*this, cond, target)); }

	value_node *insert_read_pc() { return insert(new read_pc_node(*this)); }
	action_node *insert_write_pc(port &value) { return insert(new write_pc_node(*this, value)); }

	value_node *insert_read_reg(const value_type &vt, unsigned long regoff) { return insert(new read_reg_node(*this, vt, regoff)); }
	action_node *insert_write_reg(unsigned long regoff, port &value) { return insert(new write_reg_node(*this, regoff, value)); }
	value_node *insert_read_mem(const value_type &vt, port &addr) { return insert(new read_mem_node(*this, vt, addr)); }
	action_node *insert_write_mem(port &addr, port &value) { return insert(new write_mem_node(*this, addr, value)); }

	value_node *insert_add(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::add, lhs, rhs)); }
	value_node *insert_adc(port &lhs, port &rhs, port &top) { return insert(new ternary_arith_node(*this, ternary_arith_op::adc, lhs, rhs, top)); }
	value_node *insert_sub(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::sub, lhs, rhs)); }
	value_node *insert_sbb(port &lhs, port &rhs, port &top) { return insert(new ternary_arith_node(*this, ternary_arith_op::sbb, lhs, rhs, top)); }
	value_node *insert_mul(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::mul, lhs, rhs)); }
	value_node *insert_div(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::div, lhs, rhs)); }
	value_node *insert_and(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::band, lhs, rhs)); }
	value_node *insert_or(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bor, lhs, rhs)); }
	value_node *insert_xor(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bxor, lhs, rhs)); }
	value_node *insert_not(port &lhs) { return insert(new unary_arith_node(*this, unary_arith_op::bnot, lhs)); }
	value_node *insert_cmpeq(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::cmpeq, lhs, rhs)); }
	value_node *insert_cmpne(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::cmpne, lhs, rhs)); }

	value_node *insert_zx(const value_type &target_type, port &value)
	{
		if (target_type.width() == value.type().width()) {
			return value.owner();
		}

		return insert(new cast_node(*this, cast_op::zx, target_type, value));
	}

	value_node *insert_sx(const value_type &target_type, port &value)
	{
		if (target_type.width() == value.type().width()) {
			return value.owner();
		}

		return insert(new cast_node(*this, cast_op::sx, target_type, value));
	}

	value_node *insert_trunc(const value_type &target_type, port &value) { return insert(new cast_node(*this, cast_op::trunc, target_type, value)); }
	value_node *insert_bitcast(const value_type &target_type, port &value) { return insert(new cast_node(*this, cast_op::bitcast, target_type, value)); }

	value_node *insert_csel(port &condition, port &trueval, port &falseval) { return insert(new csel_node(*this, condition, trueval, falseval)); }

	value_node *insert_lsl(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::lsl, input, amount)); }
	value_node *insert_lsr(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::lsr, input, amount)); }
	value_node *insert_asr(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::asr, input, amount)); }

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

	off_t address() const { return address_; }

	xed_decoded_inst_t *src_inst() const { return src_inst_; }

	bool updates_pc() const
	{
		for (action_node *a : actions_) {
			if (a->kind() == node_kinds::write_pc) {
				return true;
			}
		}

		return false;
	}

private:
	off_t address_;
	xed_decoded_inst_t *src_inst_;
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
