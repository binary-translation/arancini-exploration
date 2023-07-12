#pragma once

#include <arancini/ir/node.h>

namespace arancini::ir {
enum packet_type { normal, end_of_block };

class internal_function_resolver;

class ir_builder {
public:
	ir_builder(internal_function_resolver &ifr)
		: ifr_(ifr)
	{
	}

	internal_function_resolver &ifr() const { return ifr_; }

	virtual void begin_chunk() = 0;
	virtual void end_chunk() = 0;

	virtual void begin_packet(off_t address, const std::string &disassembly = "") = 0;
	virtual packet_type end_packet() = 0;

	virtual ~ir_builder() = default;

	/* Nodes */

	/// @brief Returns a node representing an integer constant.  Works for constants up to 64-bits in length.
	/// @param vt The type of the constant.
	/// @param cv The integer value of the constant.
	/// @return A constant node.
	value_node *insert_constant_i(const value_type &vt, unsigned long cv) { return create_and_insert<constant_node>(vt, cv); }

	/// @brief Returns a node representing an floating-point constant.  Works for constants up to a "double" in length.
	/// @param vt The type of the constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f(const value_type &vt, double cv) { return create_and_insert<constant_node>(vt, cv); }

	/// @brief Returns a node representing an 1-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u1(unsigned char cv) { return create_and_insert<constant_node>(value_type::u1(), (unsigned long)cv); }

	/// @brief Returns a node representing an 8-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u8(unsigned char cv) { return create_and_insert<constant_node>(value_type::u8(), (unsigned long)cv); }

	/// @brief Returns a node representing an 8-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s8(signed int cv) { return create_and_insert<constant_node>(value_type::s8(), (unsigned long)cv); }

	/// @brief Returns a node representing an 16-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u16(unsigned short cv) { return create_and_insert<constant_node>(value_type::u16(), (unsigned long)cv); }

	/// @brief Returns a node representing an 16-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s16(signed int cv) { return create_and_insert<constant_node>(value_type::s16(), (unsigned long)cv); }

	/// @brief Returns a node representing an 32-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u32(unsigned int cv) { return create_and_insert<constant_node>(value_type::u32(), (unsigned long)cv); }

	/// @brief Returns a node representing an 32-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s32(signed int cv) { return create_and_insert<constant_node>(value_type::s32(), (unsigned long)cv); }

	/// @brief Returns a node representing an 64-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u64(unsigned long cv) { return create_and_insert<constant_node>(value_type::u64(), (unsigned long)cv); }

	/// @brief Returns a node representing an 64-bit signed integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_s64(unsigned long cv) { return create_and_insert<constant_node>(value_type::s64(), (unsigned long)cv); }

	/// @brief Returns a node representing an 128-bit unsigned integer constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_u128(unsigned long cv) { return create_and_insert<constant_node>(value_type::u128(), (unsigned long)cv); }

	/// @brief Returns a node representing an 32-bit floating point constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f32(float cv) { return create_and_insert<constant_node>(value_type::f32(), (double)cv); }

	/// @brief Returns a node representing an 64-bit floating point constant.
	/// @param cv The value of the constant.
	/// @return A constant node.
	value_node *insert_constant_f64(double cv) { return create_and_insert<constant_node>(value_type::f64(), (double)cv); }

	/// @brief Returns a node representing a label used for internal control-flow puproses.
	/// @return A label node.
	label_node *insert_label() { return create_and_insert<label_node>(); }
	label_node *insert_label(std::string name) { return create_and_insert<label_node>(name); }

	/// @brief Return a node representing an unconditional branch
	/// @param target The label node that is the target of the branch
	/// @return An action node
	action_node *insert_br(label_node *target) { return create_and_insert<br_node>(target); }

	/// @brief Returns a node representing a conditional branch.  The branch will be taken if the condition evaluates to true.
	/// @param cond The condition on which to branch.  Non-zero means to take the branch, zero means to NOT take the branch.
	/// @param target The label node that is the target of the branch.
	/// @return An action node.
	action_node *insert_cond_br(port &cond, label_node *target) { return create_and_insert<cond_br_node>(cond, target); }

	/// @brief Returns a node representing a read of the emulated program counter.
	/// @return A read pc node.
	value_node *insert_read_pc() { return create_and_insert<read_pc_node>(); }

	/// @brief Returns a node representing a write of the emulated program counter.
	/// @param value The new value of the emulated PC.
	/// @return A write pc node.
	action_node *insert_write_pc(port &value) { return create_and_insert<write_pc_node>(value); }

	/// @brief read register
	/// @param vt value type
	/// @param regoff register offset (use reg_offsets enum values)
	/// @return a read register node: (vt) regoff
	value_node *insert_read_reg(const value_type &vt, unsigned long regoff, unsigned long regidx, const char *regname)
	{
		return create_and_insert<read_reg_node>(vt, regoff, regidx, regname);
	}

	/// @brief write register
	/// @param regoff register offset (use reg_offsets enum values)
	/// @param value value to write
	/// @return a write register node: regoff := value
	action_node *insert_write_reg(unsigned long regoff, unsigned long regidx, const char *regname, port &value)
	{
		return create_and_insert<write_reg_node>(regoff, regidx, regname, value);
	}

	/// @brief read memory location
	/// @param vt value type
	/// @param addr target address
	/// @return a read memory node: (vt) [addr]
	value_node *insert_read_mem(const value_type &vt, port &addr) { return create_and_insert<read_mem_node>(vt, addr); }

	/// @brief write memory location
	/// @param addr target address
	/// @param value value to write
	/// @return a write memory node: [addr] := value
	action_node *insert_write_mem(port &addr, port &value) { return create_and_insert<write_mem_node>(addr, value); }

	/// @brief addition
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return an add node: lhs + rhs
	value_node *insert_add(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::add, lhs, rhs); }

	value_node *insert_adc(port &lhs, port &rhs, port &top) { return create_and_insert<ternary_arith_node>(ternary_arith_op::adc, lhs, rhs, top); }

	/// @brief substraction
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return a sub node: lhs - rhs
	value_node *insert_sub(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::sub, lhs, rhs); }
	value_node *insert_sbb(port &lhs, port &rhs, port &top) { return create_and_insert<ternary_arith_node>(ternary_arith_op::sbb, lhs, rhs, top); }

	/// @brief multiplication
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return a mul node: lhs * rhs
	value_node *insert_mul(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::mul, lhs, rhs); }

	/// @brief division
	/// @param lhs dividend
	/// @param rhs divisor
	/// @return a div node: lhs / rhs
	value_node *insert_div(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::div, lhs, rhs); }

	/// @brief modulo
	/// @param lhs dividend
	/// @param rhs divisor
	/// @return a mod node: lhs % rhs
	value_node *insert_mod(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::mod, lhs, rhs); }

	/// @brief bitwise and
	/// @param lhs first operand
	/// @param rhs second operand
	/// @return an and node: lhs & rhs
	value_node *insert_and(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::band, lhs, rhs); }

	/// @brief bitwise or
	/// @param lhs
	/// @param rhs
	/// @return an or node: lhs | rhs
	value_node *insert_or(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::bor, lhs, rhs); }

	/// @brief bitwise xor
	/// @param lhs
	/// @param rhs
	/// @return an xor node: lhs ^ rhs
	value_node *insert_xor(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::bxor, lhs, rhs); }

	/// @brief bitwise not
	/// @param lhs
	/// @return a not node: ~lhs
	value_node *insert_not(port &lhs) { return create_and_insert<unary_arith_node>(unary_arith_op::bnot, lhs); }

	/// @brief compare equal
	/// @param lhs
	/// @param rhs
	/// @return a cmpeq node: lhs == rhs
	value_node *insert_cmpeq(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::cmpeq, lhs, rhs); }

	/// @brief compare not equal
	/// @param lhs
	/// @param rhs
	/// @return a cmpne node: lhs != rhs
	value_node *insert_cmpne(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::cmpne, lhs, rhs); }

	/// @brief compare greater than
	/// @param lhs
	/// @param rhs
	/// @return a cmpgt node: lhs > rhs
	value_node *insert_cmpgt(port &lhs, port &rhs) { return create_and_insert<binary_arith_node>(binary_arith_op::cmpgt, lhs, rhs); }

	/// @brief generic atomic binary node creator
	/// @param op: type of binary atomic operation
	/// @param mem: a memory address for the destination operand
	/// @param reg: a register for a source operand
	/// @return an atomic binary operation node that applies: [mem] := [mem] op reg
	action_node *insert_atomic_binop(binary_atomic_op op, port &mem, port &reg) { return create_and_insert<binary_atomic_node>(op, mem, reg); }

	/// @brief exchange and add
	/// @param mem: memory address of the destination operand
	/// @param reg: register used as a source operand
	/// @return an atomic xadd node:
	///         tmp := [mem] + reg
	///         reg := [mem]
	///         [mem] := tmp
	action_node *insert_atomic_xadd(port &mem, port &reg) { return create_and_insert<binary_atomic_node>(binary_atomic_op::xadd, mem, reg); }

	/// @brief atomic exchange
	/// @param mem: the memory address used in the instruction
	/// @param reg: the register used in the instruction
	/// @return an atomic xchg node: [mem] := reg, reg := [mem]
	action_node *insert_atomic_xchg(port &mem, port &reg) { return create_and_insert<binary_atomic_node>(binary_atomic_op::xchg, mem, reg); }

	/// @brief atomic compare-exchange
	/// @param dst: destination operand, needs to be a memory address
	/// @param acc: accumulator, needs to be a register
	/// @param src: source operand, needs to be a register
	/// @return an atomic cmpxchg node with the following behavior:
	///         if acc == [dst]
	///            [dst] := src
	///         else
	///            acc := [dst]
	action_node *insert_atomic_cmpxchg(port &dst, port &acc, port &src)
	{
		return create_and_insert<ternary_atomic_node>(ternary_atomic_op::cmpxchg, dst, acc, src);
	}

	/// @brief zero extend a value to a larger width (does not preserve the sign)
	/// @param target_type new type
	/// @param value value to extend
	/// @return a zx node
	value_node *insert_zx(const value_type &target_type, port &value)
	{
		if (target_type.width() == value.type().width()) {
			return value.owner();
		}

		return create_and_insert<cast_node>(cast_op::zx, target_type, value);
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

		return create_and_insert<cast_node>(cast_op::sx, target_type, value);
	}

	/// @brief truncate a value to a smaller width
	/// @param target_type new type
	/// @param value value to truncate
	/// @return a trunc node
	value_node *insert_trunc(const value_type &target_type, port &value) { return create_and_insert<cast_node>(cast_op::trunc, target_type, value); }

	value_node *insert_bitcast(const value_type &target_type, port &value) { return create_and_insert<cast_node>(cast_op::bitcast, target_type, value); }

	/// @brief Converts a value between type classes, e.g. floating point to integer.  This is NOT a bit cast, it's a value cast.
	/// @param target_type The type to convert to
	/// @param value The value being converted
	/// @return A cast node
	value_node *insert_convert(const value_type &target_type, port &value) { return create_and_insert<cast_node>(cast_op::convert, target_type, value); }
	value_node *insert_convert(const value_type &target_type, port &value, fp_convert_type convert_type)
	{
		return create_and_insert<cast_node>(cast_op::convert, target_type, value, convert_type);
	}

	value_node *insert_csel(port &condition, port &trueval, port &falseval) { return create_and_insert<csel_node>(condition, trueval, falseval); }

	/// @brief logical shift left
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an lsl node: input << amount
	value_node *insert_lsl(port &input, port &amount) { return create_and_insert<bit_shift_node>(shift_op::lsl, input, amount); }

	/// @brief logical shift right, clears the sign
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an lsr node: input >> amount
	value_node *insert_lsr(port &input, port &amount) { return create_and_insert<bit_shift_node>(shift_op::lsr, input, amount); }

	/// @brief arithmetic shift right, preserving the sign
	/// @param input value to shift
	/// @param amount number of bits to shift
	/// @return an asr node: input >> amount
	value_node *insert_asr(port &input, port &amount) { return create_and_insert<bit_shift_node>(shift_op::asr, input, amount); }

	/// @brief Returns a node representing an extraction of a series of bits from a value.  The input value has bits at position [from:from+length] extracted
	/// and returned as a value.
	/// @param input The value containing the bits to be extracted.
	/// @param from The (zero based) index of the bit to start extracting from.
	/// @param length The number of bits to extract.
	/// @return A bit extract node.
	value_node *insert_bit_extract(port &input, int from, int length) { return create_and_insert<bit_extract_node>(input, from, length); }

	/// @brief Returns a node representing an insertion of a series of bits into a value.  The input value has the bits inserted at position [to:to+length], and
	/// returned as a value.
	/// @param input The original value.
	/// @param bits The bits to insert.
	/// @param to The (zero based) index of the bit to start inserting to.
	/// @param length The number of bits to insert.
	/// @return A bit insert node.
	value_node *insert_bit_insert(port &input, port &bits, int to, int length) { return create_and_insert<bit_insert_node>(input, bits, to, length); }

	/// @brief Extract an element from a vector.
	/// @param input The vector to be extracted from.
	/// @param index The (zero based) index of the element to be extracted.
	/// @return A vector extract node.
	value_node *insert_vector_extract(port &input, int index) { return create_and_insert<vector_extract_node>(input, index); };

	/// @brief Insert an element into a vector.
	/// @param input The vector to be inserted into.
	/// @param index The (zero based) index where the element will be inserted.
	/// @param value The element to be inserted.
	/// @return A vector insert node.
	value_node *insert_vector_insert(port &input, int index, port &value) { return create_and_insert<vector_insert_node>(input, index, value); };

	value_node *insert_read_local(const local_var *local) { return create_and_insert<read_local_node>(local); }
	action_node *insert_write_local(const local_var *local, port &value) { return create_and_insert<write_local_node>(local, value); }

	virtual const local_var *alloc_local(const value_type &type) = 0;

	action_node *insert_internal_call(const internal_function &fn, const std::vector<port *> &args) { return create_and_insert<internal_call_node>(fn, args); }

protected:
	virtual void insert_action(action_node *a) = 0;
	virtual void process_node(node *) {};

private:
	internal_function_resolver &ifr_;

	template <class T, typename... Args> T *create_and_insert(Args &&...args) { return insert(new T(std::forward<Args>(args)...)); }

	template <class T> T *insert(T *n)
	{
		process_node(n);

		if (n->is_action()) {
			insert_action((action_node *)n);
		}

		return n;
	}
};
} // namespace arancini::ir
