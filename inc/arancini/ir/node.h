#pragma once

#include <stdexcept>
#include <vector>

#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
enum class node_kinds {
	label,
	read_pc,
	write_pc,
	constant,
	unary_arith,
	binary_arith,
	ternary_arith,
	read_reg,
	read_mem,
	write_reg,
	write_mem,
	cast,
	csel,
	bit_shift,
	cond_br,
	vector_extract,
	vector_insert
};

class packet;

class node {
public:
	node(packet &owner, node_kinds kind)
		: owner_(owner)
		, kind_(kind)
	{
	}

	node_kinds kind() const { return kind_; }

	virtual bool is_action() const { return false; }

	virtual bool accept(visitor &v) { return v.visit_node(*this); }

private:
	packet &owner_;
	node_kinds kind_;
};

class action_node : public node {
public:
	action_node(packet &owner, node_kinds kind)
		: node(owner, kind)
	{
	}

	virtual bool is_action() const override { return true; }

	virtual bool updates_pc() const { return false; }

	virtual bool accept(visitor &v) override
	{
		if (!node::accept(v)) {
			return false;
		}

		return v.visit_action_node(*this);
	}
};

class label_node : public action_node {
public:
	label_node(packet &owner)
		: action_node(owner, node_kinds::label)
	{
	}
};

class value_node : public node {
public:
	value_node(packet &owner, node_kinds kind, const value_type &vt)
		: node(owner, kind)
		, value_(port_kinds::value, vt, this)
	{
	}

	port &val() { return value_; }
	const port &val() const { return value_; }

	virtual bool accept(visitor &v) override
	{
		if (!node::accept(v)) {
			return false;
		}

		if (!v.visit_value_node(*this)) {
			return false;
		}

		return value_.accept(v);
	}

private:
	port value_;
};

class cond_br_node : public action_node {
public:
	cond_br_node(packet &owner, port &cond, label_node *target)
		: action_node(owner, node_kinds::cond_br)
		, cond_(cond)
		, target_(target)
	{
	}

	port &cond() const { return cond_; }
	label_node *target() const { return target_; }

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		return v.visit_cond_br_node(*this);
	}

private:
	port &cond_;
	label_node *target_;
};

class read_pc_node : public value_node {
public:
	read_pc_node(packet &owner)
		: value_node(owner, node_kinds::read_pc, value_type::u64())
	{
	}

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		return v.visit_read_pc_node(*this);
	}
};

class write_pc_node : public action_node {
public:
	write_pc_node(packet &owner, port &value)
		: action_node(owner, node_kinds::write_pc)
		, value_(value)
	{
		value.add_target(this);
	}

	port &value() const { return value_; }

	virtual bool updates_pc() const override { return true; }

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		if (!v.visit_write_pc_node(*this)) {
			return false;
		}

		return value_.owner()->accept(v);
	}

private:
	port &value_;
};

class constant_node : public value_node {
public:
	constant_node(packet &owner, const value_type &vt, unsigned long cv)
		: value_node(owner, node_kinds::constant, vt)
		, cvi_(cv)
	{
		if (vt.type_class() != value_type_class::signed_integer && vt.type_class() != value_type_class::unsigned_integer) {
			throw std::runtime_error("constructing a constant node with an integer for a non-integer value type");
		}
	}

	constant_node(packet &owner, const value_type &vt, double cv)
		: value_node(owner, node_kinds::constant, vt)
		, cvf_(cv)
	{
		if (vt.type_class() != value_type_class::floating_point) {
			throw std::runtime_error("constructing a constant node with a float for a non-float value type");
		}
	}

	unsigned long const_val_i() const { return cvi_; }
	double const_val_f() const { return cvf_; }

	bool is_zero() const { return val().type().is_floating_point() ? cvf_ == 0 : cvi_ == 0; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		return v.visit_constant_node(*this);
	}

private:
	union {
		unsigned long cvi_;
		double cvf_;
	};
};

class read_reg_node : public value_node {
public:
	read_reg_node(packet &owner, const value_type &vt, unsigned long regoff)
		: value_node(owner, node_kinds::read_reg, vt)
		, regoff_(regoff)
	{
	}

	unsigned long regoff() const { return regoff_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		return v.visit_read_reg_node(*this);
	}

private:
	unsigned long regoff_;
};

class read_mem_node : public value_node {
public:
	read_mem_node(packet &owner, const value_type &vt, port &addr)
		: value_node(owner, node_kinds::read_mem, vt)
		, addr_(addr)
	{
		addr.add_target(this);
	}

	port &address() const { return addr_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		if (!v.visit_read_mem_node(*this)) {
			return false;
		}

		return addr_.owner()->accept(v);
	}

private:
	port &addr_;
};

class write_reg_node : public action_node {
public:
	write_reg_node(packet &owner, unsigned long regoff, port &val)
		: action_node(owner, node_kinds::write_reg)
		, regoff_(regoff)
		, val_(val)
	{
		val.add_target(this);
	}

	unsigned long regoff() const { return regoff_; }
	port &value() const { return val_; }

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		if (!v.visit_write_reg_node(*this)) {
			return false;
		}

		return val_.owner()->accept(v);
	}

private:
	unsigned long regoff_;
	port &val_;
};

class write_mem_node : public action_node {
public:
	write_mem_node(packet &owner, port &addr, port &val)
		: action_node(owner, node_kinds::write_mem)
		, addr_(addr)
		, val_(val)
	{
		addr.add_target(this);
		val.add_target(this);
	}

	port &address() const { return addr_; }
	port &value() const { return val_; }

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		if (!v.visit_write_mem_node(*this)) {
			return false;
		}

		if (!addr_.owner()->accept(v)) {
			return false;
		}

		return val_.owner()->accept(v);
	}

private:
	port &addr_;
	port &val_;
};

class csel_node : public value_node {
public:
	csel_node(packet &owner, port &condition, port &trueval, port &falseval)
		: value_node(owner, node_kinds::csel, trueval.type())
		, condition_(condition)
		, trueval_(trueval)
		, falseval_(falseval)
	{
		condition.add_target(this);
		trueval.add_target(this);
		falseval.add_target(this);
	}

	port &condition() const { return condition_; }
	port &trueval() const { return trueval_; }
	port &falseval() const { return falseval_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		if (!v.visit_csel_node(*this)) {
			return false;
		}

		if (!condition_.owner()->accept(v)) {
			return false;
		}

		if (!trueval_.owner()->accept(v)) {
			return false;
		}

		if (!falseval_.owner()->accept(v)) {
			return false;
		}

		return true;
	}

private:
	port &condition_;
	port &trueval_;
	port &falseval_;
};

enum class shift_op { lsl, lsr, asr };

class bit_shift_node : public value_node {
public:
	bit_shift_node(packet &owner, shift_op op, port &input, port &amount)
		: value_node(owner, node_kinds::bit_shift, input.type())
		, op_(op)
		, input_(input)
		, amount_(amount)
	{
		input.add_target(this);
		amount.add_target(this);
	}

	shift_op op() const { return op_; }

	port &input() const { return input_; }
	port &amount() const { return amount_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		if (!v.visit_bit_shift_node(*this)) {
			return false;
		}

		if (!input_.owner()->accept(v)) {
			return false;
		}

		return amount_.owner()->accept(v);
	}

private:
	shift_op op_;
	port &input_;
	port &amount_;
};

enum class cast_op { bitcast, zx, sx, trunc };

class cast_node : public value_node {
public:
	cast_node(packet &owner, cast_op op, const value_type &target_type, port &source_value)
		: value_node(owner, node_kinds::cast, target_type)
		, op_(op)
		, target_type_(target_type)
		, source_value_(source_value)
	{
		if (op == cast_op::bitcast) {
			if (target_type.width() != source_value.type().width()) {
				throw std::logic_error(
					"cannot bitcast between types with different sizes target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
			}
		} else {
			if (target_type.type_class() != source_value.type().type_class()) {
				throw std::logic_error("cannot cast between type classes target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
			}
		}

		source_value.add_target(this);
	}

	cast_op op() const { return op_; }

	port &source_value() const { return source_value_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		if (!v.visit_cast_node(*this)) {
			return false;
		}

		return source_value_.owner()->accept(v);
	}

private:
	cast_op op_;
	value_type target_type_;
	port &source_value_;
};

class arith_node : public value_node {
public:
	arith_node(packet &owner, node_kinds kind, const value_type &type)
		: value_node(owner, kind, type)
		, zero_(port_kinds::zero, value_type::u1(), this)
		, negative_(port_kinds::negative, value_type::u1(), this)
		, overflow_(port_kinds::overflow, value_type::u1(), this)
		, carry_(port_kinds::carry, value_type::u1(), this)
	{
	}

	port &zero() { return zero_; }
	port &negative() { return negative_; }
	port &overflow() { return overflow_; }
	port &carry() { return carry_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		if (!v.visit_arith_node(*this)) {
			return false;
		}

		if (!zero_.accept(v)) {
			return false;
		}

		if (!negative_.accept(v)) {
			return false;
		}

		if (!overflow_.accept(v)) {
			return false;
		}

		if (!carry_.accept(v)) {
			return false;
		}

		return true;
	}

private:
	port zero_, negative_, overflow_, carry_;
};

enum class unary_arith_op { bnot, neg, complement };

class unary_arith_node : public arith_node {
public:
	unary_arith_node(packet &owner, unary_arith_op op, port &lhs)
		: arith_node(owner, node_kinds::unary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
	{
		lhs.add_target(this);
	}

	unary_arith_op op() const { return op_; }

	port &lhs() const { return lhs_; }

	virtual bool accept(visitor &v) override
	{
		if (!arith_node::accept(v)) {
			return false;
		}

		if (!v.visit_unary_arith_node(*this)) {
			return false;
		}

		if (!lhs_.owner()->accept(v)) {
			return false;
		}

		return true;
	}

private:
	unary_arith_op op_;
	port &lhs_;
};

enum class binary_arith_op { add, sub, mul, div, band, bor, bxor, cmpeq, cmpne };

class binary_arith_node : public arith_node {
public:
	binary_arith_node(packet &owner, binary_arith_op op, port &lhs, port &rhs)
		: arith_node(owner, node_kinds::binary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
		, rhs_(rhs)

	{
		if (!lhs.type().equivalent_to(rhs.type())) {
			throw std::logic_error("incompatible types in binary arith node: lhs=" + lhs.type().to_string() + ", rhs=" + rhs.type().to_string());
		}

		lhs.add_target(this);
		rhs.add_target(this);
	}

	binary_arith_op op() const { return op_; }

	port &lhs() const { return lhs_; }
	port &rhs() const { return rhs_; }

	virtual bool accept(visitor &v) override
	{
		if (!arith_node::accept(v)) {
			return false;
		}

		if (!v.visit_binary_arith_node(*this)) {
			return false;
		}

		if (!lhs_.owner()->accept(v)) {
			return false;
		}

		if (!rhs_.owner()->accept(v)) {
			return false;
		}

		return true;
	}

private:
	binary_arith_op op_;
	port &lhs_;
	port &rhs_;
};

enum class ternary_arith_op { adc, sbb };

class ternary_arith_node : public arith_node {
public:
	ternary_arith_node(packet &owner, ternary_arith_op op, port &lhs, port &rhs, port &top)
		: arith_node(owner, node_kinds::ternary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
		, rhs_(rhs)
		, top_(top)
	{
		lhs.add_target(this);
		rhs.add_target(this);
		top.add_target(this);
	}

	ternary_arith_op op() const { return op_; }

	port &lhs() const { return lhs_; }
	port &rhs() const { return rhs_; }
	port &top() const { return top_; }

	virtual bool accept(visitor &v) override
	{
		if (!arith_node::accept(v)) {
			return false;
		}

		if (!v.visit_ternary_arith_node(*this)) {
			return false;
		}

		if (!lhs_.owner()->accept(v)) {
			return false;
		}

		if (!rhs_.owner()->accept(v)) {
			return false;
		}

		if (!top_.owner()->accept(v)) {
			return false;
		}

		return true;
	}

private:
	ternary_arith_op op_;
	port &lhs_;
	port &rhs_;
	port &top_;
};

class vector_node : public value_node {
public:
	vector_node(packet &owner, node_kinds kind, const value_type &type, port &vct)
		: value_node(owner, kind, type)
		, vct_(vct)
	{
	}

private:
	port &vct_;
	int index_;
};

class vector_element_node : public vector_node {
public:
	vector_element_node(packet &owner, node_kinds kind, const value_type &type, port &vct, int index)
		: vector_node(owner, kind, type, vct)
		, index_(index)
	{
	}

private:
	int index_;
};

class vector_extract_node : public vector_element_node {
public:
	vector_extract_node(packet &owner, port &vct, int index)
		: vector_element_node(owner, node_kinds::vector_extract, vct.type().element_type(), vct, index)
	{
	}
};

class vector_insert_node : public vector_element_node {
public:
	vector_insert_node(packet &owner, port &vct, int index, port &val)
		: vector_element_node(owner, node_kinds::vector_insert, vct.type(), vct, index)
		, val_(val)
	{
	}

private:
	port &val_;
};
} // namespace arancini::ir
