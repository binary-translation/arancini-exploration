#pragma once

#include <string>
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
		: address_(address)
	{
		char buffer[64];
		xed_format_context(XED_SYNTAX_INTEL, src_inst, buffer, sizeof(buffer), address, nullptr, 0);
		src_inst_str_ = std::string(buffer);
	}

	/// @brief Returns a node representing an integer constant.  Works for constants up to 64-bits in length.
	/// @param vt The type of the constant.
	/// @param cv The integer value of the constant.
	/// @return A constant node.
	value_node *insert_constant_i(const value_type &vt, unsigned long cv) { return insert(new constant_node(*this, vt, cv)); }

	/// @brief Returns a node representing an floating-point constant.  Works for constants up to a "double" in length.
	/// @param vt The type of the constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f(const value_type &vt, double cv) { return insert(new constant_node(*this, vt, cv)); }

	/// @brief Returns a node representing an 8-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u8(unsigned char cv) { return insert(new constant_node(*this, value_type::u8(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 16-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u16(unsigned short cv) { return insert(new constant_node(*this, value_type::u16(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 32-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u32(unsigned int cv) { return insert(new constant_node(*this, value_type::u32(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 32-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s32(signed int cv) { return insert(new constant_node(*this, value_type::s32(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 64-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u64(unsigned long cv) { return insert(new constant_node(*this, value_type::u64(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 64-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s64(unsigned long cv) { return insert(new constant_node(*this, value_type::s64(), (unsigned long)cv)); }

	/// @brief Returns a node representing an 32-bit floating point constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f32(float cv) { return insert(new constant_node(*this, value_type::f32(), (double)cv)); }

	/// @brief Returns a node representing an 64-bit floating point constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f64(double cv) { return insert(new constant_node(*this, value_type::f64(), (double)cv)); }

	/// @brief Returns a node representing a label used for internal control-flow puproses.
	/// @return A label node.
	label_node *insert_label() { return insert(new label_node(*this)); }

	/// @brief Returns a node representing a conditional branch.  The branch will be taken if the condition evaluates to true.
	/// @param cond The condition on which to branch.  Non-zero means to take the branch, zero means to NOT take the branch.
	/// @param target The label node that is the target of the branch.
	/// @return An action node.
	action_node *insert_cond_br(port &cond, label_node *target) { return insert(new cond_br_node(*this, cond, target)); }

	/// @brief Returns a node representing a read of the emulated program counter.
	/// @return A read pc node.
	value_node *insert_read_pc() { return insert(new read_pc_node(*this)); }

	/// @brief Returns a node representing a write of the emulated program counter.
	/// @param value The new value of the emulated PC.
	/// @return A write pc node.
	action_node *insert_write_pc(port &value) { return insert(new write_pc_node(*this, value)); }

	/// @brief read register
	/// @param vt value type
	/// @param regoff register offset (use reg_offsets enum values)
	/// @return a read register node: (vt) regoff
	value_node *insert_read_reg(const value_type &vt, unsigned long regoff) { return insert(new read_reg_node(*this, vt, regoff)); }

	/// @brief write register
	/// @param regoff register offset (use reg_offsets enum values)
	/// @param value value to write
	/// @return a write register node: regoff := value
	action_node *insert_write_reg(unsigned long regoff, port &value) { return insert(new write_reg_node(*this, regoff, value)); }

	/// @brief read memory location
	/// @param vt value type
	/// @param addr target address
	/// @return a read memory node: (vt) [addr]
	value_node *insert_read_mem(const value_type &vt, port &addr) { return insert(new read_mem_node(*this, vt, addr)); }

	/// @brief write memory location
	/// @param addr target address
	/// @param value value to write
	/// @return a write memory node: [addr] := value
	action_node *insert_write_mem(port &addr, port &value) { return insert(new write_mem_node(*this, addr, value)); }

	/// @brief addition
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return an add node: lhs + rhs
	value_node *insert_add(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::add, lhs, rhs)); }

	value_node *insert_adc(port &lhs, port &rhs, port &top) { return insert(new ternary_arith_node(*this, ternary_arith_op::adc, lhs, rhs, top)); }

	/// @brief substraction
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return a sub node: lhs - rhs
	value_node *insert_sub(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::sub, lhs, rhs)); }
	value_node *insert_sbb(port &lhs, port &rhs, port &top) { return insert(new ternary_arith_node(*this, ternary_arith_op::sbb, lhs, rhs, top)); }

	/// @brief multiplication
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return a mul node: lhs * rhs
	value_node *insert_mul(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::mul, lhs, rhs)); }

	/// @brief division
	/// @param lhs dividend
	/// @param rhs divisor
	/// @return a div node lhs / rhs
	value_node *insert_div(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::div, lhs, rhs)); }

	/// @brief bitwise and
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return an and node: lhs & rhs
	value_node *insert_and(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::band, lhs, rhs)); }

	/// @brief bitwise or
	/// @param lhs
	/// @param rhs
	/// @return an or node: lhs | rhs
	value_node *insert_or(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bor, lhs, rhs)); }

	/// @brief bitwise xor
	/// @param lhs
	/// @param rhs
	/// @return an xor node: lhs ^ rhs
	value_node *insert_xor(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::bxor, lhs, rhs)); }

	/// @brief bitwise not
	/// @param lhs
	/// @return a not node: ~lhs
	value_node *insert_not(port &lhs) { return insert(new unary_arith_node(*this, unary_arith_op::bnot, lhs)); }

	/// @brief compare equal
	/// @param lhs
	/// @param rhs
	/// @return a cmpeq node: lhs == rhs
	value_node *insert_cmpeq(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::cmpeq, lhs, rhs)); }

	/// @brief compare not equal
	/// @param lhs
	/// @param rhs
	/// @return a cmpne node: lhs != rhs
	value_node *insert_cmpne(port &lhs, port &rhs) { return insert(new binary_arith_node(*this, binary_arith_op::cmpne, lhs, rhs)); }

	/// @brief zero extend a value to a larger width (does not preserve the sign)
	/// @param target_type new type
	/// @param value value to extend
	/// @return a zx node
	value_node *insert_zx(const value_type &target_type, port &value)
	{
		if (target_type.width() == value.type().width()) {
			return value.owner();
		}

		return insert(new cast_node(*this, cast_op::zx, target_type, value));
	}

	/// @brief sign extend a value to a larger width (preserves the sign)
	/// @param target_type new type
	/// @param value value to extend
	/// @return an sx node
	value_node *insert_sx(const value_type &target_type, port &value)
	{
		if (target_type.width() == value.type().width()) {
			return value.owner();
		}

		return insert(new cast_node(*this, cast_op::sx, target_type, value));
	}

	/// @brief truncate a value to a smaller width
	/// @param target_type new type
	/// @param value value to truncate
	/// @return a trunc node
	value_node *insert_trunc(const value_type &target_type, port &value) { return insert(new cast_node(*this, cast_op::trunc, target_type, value)); }

	value_node *insert_bitcast(const value_type &target_type, port &value) { return insert(new cast_node(*this, cast_op::bitcast, target_type, value)); }

	/// @brief Converts a value between type classes, e.g. floating point to integer.  This is NOT a bit cast, it's a value cast.
	/// @param target_type The type to convert to
	/// @param value The value being converted
	/// @return A cast node
	value_node *insert_convert(const value_type &target_type, port &value) { return insert(new cast_node(*this, cast_op::convert, target_type, value)); }

	value_node *insert_csel(port &condition, port &trueval, port &falseval) { return insert(new csel_node(*this, condition, trueval, falseval)); }

	/// @brief logical shift left
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an lsl node: input << amount
	value_node *insert_lsl(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::lsl, input, amount)); }

	/// @brief logical shift right, clears the sign
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an lsr node: input >> amount
	value_node *insert_lsr(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::lsr, input, amount)); }

	/// @brief arithmetic shift right, preserving the sign
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an asr node: input >> amount
	value_node *insert_asr(port &input, port &amount) { return insert(new bit_shift_node(*this, shift_op::asr, input, amount)); }

	/// @brief Returns a node representing an extraction of a series of bits from a value.  The input value has bits at position [from:from+length] extracted and returned as a value.
	/// @param input The value containing the bits to be extracted.
	/// @param from The (zero based) index of the bit to start extracting from.
	/// @param length The number of bits to extract.
	/// @return A bit extract node.
	value_node *insert_bit_extract(port &input, int from, int length) { return insert(new bit_extract_node(*this, input, from, length)); }

	/// @brief Returns a node representing an insertion of a series of bits into a value.  The input value has the bits inserted at position [to:to+length], and returned as a value.
	/// @param input The original value.
	/// @param bits The bits to insert.
	/// @param to The (zero based) index of the bit to start inserting to.
	/// @param length The number of bits to insert.
	/// @return A bit insert node.
	value_node *insert_bit_insert(port &input, port &bits, int to, int length) { return insert(new bit_insert_node(*this, input, bits, to, length)); }

	/// @brief Extract an element from a vector.
	/// @param input The vector to be extracted from.
	/// @param index The (zero based) index of the element to be extracted.
	/// @return A vector extract node.
	value_node *insert_vector_extract(port &input, int index) { return insert(new vector_extract_node(*this, input, index)); };

	/// @brief Insert an element into a vector.
	/// @param input The vector to be inserted into.
	/// @param index The (zero based) index where the element will be inserted.
	/// @param value The element to be inserted.
	/// @return A vector insert node.
	value_node *insert_vector_insert(port &input, int index, port& value) { return insert(new vector_insert_node(*this,input, index, value)); };

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

	std::string src_inst_str() const { return src_inst_str_; }

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
	std::string src_inst_str_;
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
