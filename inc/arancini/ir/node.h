#pragma once

#include <vector>

#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
enum class node_kinds { start, end, read_pc, write_pc, constant, binary_arith, read_reg, read_mem, write_reg, write_mem };

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

	virtual bool accept(visitor &v) override
	{
		if (!node::accept(v)) {
			return false;
		}

		return v.visit_action_node(*this);
	}
};

class start_node : public action_node {
public:
	start_node(packet &owner, unsigned long offset)
		: action_node(owner, node_kinds::start)
		, offset_(offset)
	{
	}

	unsigned long offset() const { return offset_; }

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		return v.visit_start_node(*this);
	}

private:
	unsigned long offset_;
};

class end_node : public action_node {
public:
	end_node(packet &owner)
		: action_node(owner, node_kinds::end)
	{
	}

	virtual bool accept(visitor &v) override
	{
		if (!action_node::accept(v)) {
			return false;
		}

		return v.visit_end_node(*this);
	}
};

class value_node : public node {
public:
	value_node(packet &owner, node_kinds kind, const value_type &vt)
		: node(owner, kind)
		, value_(port_kinds::value, vt, this)
	{
	}

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
	write_pc_node(packet &owner, const port &value)
		: action_node(owner, node_kinds::write_pc)
		, value_(value)
	{
	}

	const port &value() const { return value_; }

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
	const port &value_;
};

class constant_node : public value_node {
public:
	constant_node(packet &owner, const value_type &vt, unsigned long cv)
		: value_node(owner, node_kinds::constant, vt)
		, cv_(cv)
	{
	}

	unsigned long const_val() const { return cv_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		return v.visit_constant_node(*this);
	}

private:
	unsigned long cv_;
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
	read_mem_node(packet &owner, const value_type &vt, const port &addr)
		: value_node(owner, node_kinds::read_mem, vt)
		, addr_(addr)
	{
	}

	const port &address() const { return addr_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
			return false;
		}

		return v.visit_read_mem_node(*this);
	}

private:
	const port &addr_;
};

class write_reg_node : public action_node {
public:
	write_reg_node(packet &owner, unsigned long regoff, const port &val)
		: action_node(owner, node_kinds::write_reg)
		, regoff_(regoff)
		, val_(val)
	{
	}

	unsigned long regoff() const { return regoff_; }
	const port &value() const { return val_; }

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
	const port &val_;
};

class write_mem_node : public action_node {
public:
	write_mem_node(packet &owner, const port &addr, const port &val)
		: action_node(owner, node_kinds::write_mem)
		, addr_(addr)
		, val_(val)
	{
	}

	const port &address() const { return addr_; }
	const port &value() const { return val_; }

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
	const port &addr_;
	const port &val_;
};

enum class binary_arith_op { add, sub, mul, div, band, bor, bxor };

class binary_arith_node : public value_node {
public:
	binary_arith_node(packet &owner, binary_arith_op op, const port &lhs, const port &rhs)
		: value_node(owner, node_kinds::binary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
		, rhs_(rhs)
		, zero_(port_kinds::zero, value_type::u1(), this)
		, negative_(port_kinds::negative, value_type::u1(), this)
		, overflow_(port_kinds::overflow, value_type::u1(), this)
		, carry_(port_kinds::carry, value_type::u1(), this)
	{
	}

	binary_arith_op op() const { return op_; }

	const port &lhs() const { return lhs_; }
	const port &rhs() const { return rhs_; }

	const port &zero() const { return zero_; }
	const port &negative() const { return negative_; }
	const port &overflow() const { return overflow_; }
	const port &carry() const { return carry_; }

	virtual bool accept(visitor &v) override
	{
		if (!value_node::accept(v)) {
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
	binary_arith_op op_;
	const port &lhs_;
	const port &rhs_;
	port zero_, negative_, overflow_, carry_;
};
} // namespace arancini::ir
