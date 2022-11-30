#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include <arancini/ir/metadata.h>
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
	br,
	cond_br,
	bit_extract,
	bit_insert,
	vector_extract,
	vector_insert
};

class node {
public:
	node(node_kinds kind)
		: kind_(kind)
	{
	}

	node_kinds kind() const { return kind_; }

	virtual bool is_action() const { return false; }

	virtual void accept(visitor &v) { v.visit_node(*this); }

	void set_metadata(const std::string &key, std::shared_ptr<metadata> value) { md_[key] = value; }

	std::shared_ptr<metadata> get_metadata(const std::string &key) const { return md_.at(key); }

	std::vector<std::pair<std::string, std::shared_ptr<metadata>>> get_metadata_of_kind(metadata_kind kind) const
	{
		std::vector<std::pair<std::string, std::shared_ptr<metadata>>> r;

		for (auto &n : md_) {
			if (n.second->kind() == kind) {
				r.push_back({ n.first, n.second });
			}
		}

		return r;
	}

	std::shared_ptr<metadata> try_get_metadata(const std::string &key) const
	{
		auto m = md_.find(key);

		if (m == md_.end()) {
			return nullptr;
		} else {
			return m->second;
		}
	}

	bool has_metadata(const std::string &key) const { return md_.count(key) > 0; }

private:
	node_kinds kind_;
	std::map<std::string, std::shared_ptr<metadata>> md_;
};

class action_node : public node {
public:
	action_node(node_kinds kind)
		: node(kind)
	{
	}

	virtual bool is_action() const override { return true; }

	virtual bool updates_pc() const { return false; }

	virtual void accept(visitor &v) override
	{
		node::accept(v);
		v.visit_action_node(*this);
	}
};

class label_node : public action_node {
public:
	label_node(std::string name)
		: action_node(node_kinds::label)
		, name_(name)
	{
	}

	label_node()
		: label_node("")
	{
	}

	const std::string name() { return name_; }

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_label_node(*this);
	}

	std::string name_;
};

class value_node : public node {
public:
	value_node(node_kinds kind, const value_type &vt)
		: node(kind)
		, value_(port_kinds::value, vt, this)
	{
	}

	port &val() { return value_; }
	const port &val() const { return value_; }

	virtual void accept(visitor &v) override
	{
		node::accept(v);
		v.visit_value_node(*this);
	}

protected:
	port value_;
};

class br_node : public action_node {
public:
	br_node(label_node *target)
		: action_node(node_kinds::br)
		, target_(target)
	{
	}

	label_node *target() const { return target_; }

	void add_br_target(label_node *n)
	{
		target_ = n;
	}

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_br_node(*this);
	}

private:
	label_node *target_;
};

class cond_br_node : public action_node {
public:
	cond_br_node(port &cond, label_node *target)
		: action_node(node_kinds::cond_br)
		, cond_(cond)
		, target_(target)
	{
		cond.add_target(this);
	}

	port &cond() const { return cond_; }
	label_node *target() const { return target_; }

	void add_br_target(label_node *n)
	{
		target_ = n;
	}

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_cond_br_node(*this);
	}

private:
	port &cond_;
	label_node *target_;
};

class read_pc_node : public value_node {
public:
	read_pc_node()
		: value_node(node_kinds::read_pc, value_type::u64())
	{
	}

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_read_pc_node(*this);
	}
};

class write_pc_node : public action_node {
public:
	write_pc_node(port &value)
		: action_node(node_kinds::write_pc)
		, value_(value)
	{
		value.add_target(this);
	}

	port &value() const { return value_; }

	virtual bool updates_pc() const override { return true; }

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_write_pc_node(*this);
	}

private:
	port &value_;
};

class constant_node : public value_node {
public:
	constant_node(const value_type &vt, unsigned long cv)
		: value_node(node_kinds::constant, vt)
		, cvi_(cv)
	{
		if (vt.type_class() != value_type_class::signed_integer && vt.type_class() != value_type_class::unsigned_integer) {
			throw std::runtime_error("constructing a constant node with an integer for a non-integer value type");
		}
	}

	constant_node(const value_type &vt, double cv)
		: value_node(node_kinds::constant, vt)
		, cvf_(cv)
	{
		if (vt.type_class() != value_type_class::floating_point) {
			throw std::runtime_error("constructing a constant node with a float for a non-float value type");
		}
	}

	unsigned long const_val_i() const { return cvi_; }
	double const_val_f() const { return cvf_; }

	bool is_zero() const { return val().type().is_floating_point() ? cvf_ == 0 : cvi_ == 0; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_constant_node(*this);
	}

private:
	union {
		unsigned long cvi_;
		double cvf_;
	};
};

class read_reg_node : public value_node {
public:
	read_reg_node(const value_type &vt, unsigned long regoff)
		: value_node(node_kinds::read_reg, vt)
		, regoff_(regoff)
	{
	}

	unsigned long regoff() const { return regoff_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_read_reg_node(*this);
	}

private:
	unsigned long regoff_;
};

class read_mem_node : public value_node {
public:
	read_mem_node(const value_type &vt, port &addr)
		: value_node(node_kinds::read_mem, vt)
		, addr_(addr)
	{
		addr.add_target(this);
	}

	port &address() const { return addr_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_read_mem_node(*this);
	}

private:
	port &addr_;
};

class write_reg_node : public action_node {
public:
	write_reg_node(unsigned long regoff, port &val)
		: action_node(node_kinds::write_reg)
		, regoff_(regoff)
		, val_(val)
	{
		val.add_target(this);
	}

	unsigned long regoff() const { return regoff_; }
	port &value() const { return val_; }

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_write_reg_node(*this);
	}

private:
	unsigned long regoff_;
	port &val_;
};

class write_mem_node : public action_node {
public:
	write_mem_node(port &addr, port &val)
		: action_node(node_kinds::write_mem)
		, addr_(addr)
		, val_(val)
	{
		addr.add_target(this);
		val.add_target(this);
	}

	port &address() const { return addr_; }
	port &value() const { return val_; }

	virtual void accept(visitor &v) override
	{
		action_node::accept(v);
		v.visit_write_mem_node(*this);
	}

private:
	port &addr_;
	port &val_;
};

class csel_node : public value_node {
public:
	csel_node(port &condition, port &trueval, port &falseval)
		: value_node(node_kinds::csel, trueval.type())
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

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_csel_node(*this);
	}

private:
	port &condition_;
	port &trueval_;
	port &falseval_;
};

enum class shift_op { lsl, lsr, asr };

class bit_shift_node : public value_node {
public:
	bit_shift_node(shift_op op, port &input, port &amount)
		: value_node(node_kinds::bit_shift, input.type())
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

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_bit_shift_node(*this);
	}

private:
	shift_op op_;
	port &input_;
	port &amount_;
};

enum class cast_op { bitcast, zx, sx, trunc, convert };
enum class fp_convert_type { none, round, trunc };

class cast_node : public value_node {
public:
	cast_node(cast_op op, const value_type &target_type, port &source_value, fp_convert_type convert_type)
		: value_node(node_kinds::cast, target_type)
		, op_(op)
		, target_type_(target_type)
		, source_value_(source_value)
		, convert_type_(convert_type)
	{
		if (op == cast_op::bitcast) {
			if (target_type.width() != source_value.type().width()) {
				throw std::logic_error(
					"cannot bitcast between types with different sizes target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
			}
		} else if (op == cast_op::convert) {
			if ((target_type.type_class() != value_type_class::floating_point) && (source_value.type().type_class() != value_type_class::floating_point)) {
				if (target_type.type_class() == source_value.type().type_class()) {
					throw std::logic_error(
						"cannot convert between the same non-FP type classes target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
				}
			}
		} else {
			if (target_type.type_class() != source_value.type().type_class()) {
				throw std::logic_error("cannot cast between type classes target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
			}
		}

		if ((convert_type != fp_convert_type::none) && (op != cast_op::convert)) {
			throw std::logic_error(
				"convert type should be 'none' if the cast_op is not 'convert' target=" + target_type.to_string() + ", source=" + source_value.type().to_string());
		}

		source_value.add_target(this);
	}

	cast_node(cast_op op, const value_type &target_type, port &source_value)
		: cast_node(op, target_type, source_value, fp_convert_type::none)
	{
	}

	cast_op op() const { return op_; }

	port &source_value() const { return source_value_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_cast_node(*this);
	}

private:
	cast_op op_;
	value_type target_type_;
	port &source_value_;
	fp_convert_type convert_type_;
};

class arith_node : public value_node {
public:
	arith_node(node_kinds kind, const value_type &type)
		: value_node(kind, type)
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

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_arith_node(*this);
	}

private:
	port zero_, negative_, overflow_, carry_;
};

enum class unary_arith_op { bnot, neg, complement };

class unary_arith_node : public arith_node {
public:
	unary_arith_node(unary_arith_op op, port &lhs)
		: arith_node(node_kinds::unary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
	{
		lhs.add_target(this);
	}

	unary_arith_op op() const { return op_; }

	port &lhs() const { return lhs_; }

	virtual void accept(visitor &v) override
	{
		arith_node::accept(v);
		v.visit_unary_arith_node(*this);
	}

private:
	unary_arith_op op_;
	port &lhs_;
};

enum class binary_arith_op { add, sub, mul, div, mod, band, bor, bxor, cmpeq, cmpne };

class binary_arith_node : public arith_node {
public:
	binary_arith_node(binary_arith_op op, port &lhs, port &rhs)
		: arith_node(node_kinds::binary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
		, rhs_(rhs)
	{
		if (!lhs.type().equivalent_to(rhs.type())) {
			throw std::logic_error("incompatible types in binary arith node: lhs=" + lhs.type().to_string() + ", rhs=" + rhs.type().to_string());
		}

		op_ = op;
		lhs_ = lhs;
		rhs_ = rhs;

		lhs.add_target(this);
		rhs.add_target(this);
	}

	binary_arith_op op() const { return op_; }

	port &lhs() const { return lhs_; }
	port &rhs() const { return rhs_; }

	virtual void accept(visitor &v) override
	{
		arith_node::accept(v);
		v.visit_binary_arith_node(*this);
	}

private:
	binary_arith_op op_;
	port &lhs_;
	port &rhs_;
};

enum class ternary_arith_op { adc, sbb };

class ternary_arith_node : public arith_node {
public:
	ternary_arith_node(ternary_arith_op op, port &lhs, port &rhs, port &top)
		: arith_node(node_kinds::ternary_arith, lhs.type())
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

	virtual void accept(visitor &v) override
	{
		arith_node::accept(v);
		v.visit_ternary_arith_node(*this);
	}

private:
	ternary_arith_op op_;
	port &lhs_;
	port &rhs_;
	port &top_;
};

class bit_extract_node : public value_node {
public:
	bit_extract_node(port &value, int from, int length)
		: value_node(node_kinds::bit_extract, value_type(value_type_class::unsigned_integer, length))
		, source_value_(value)
		, from_(from)
		, length_(length)
	{
		if (from + length - 1 > source_value_.type().width() - 1) {
			throw std::logic_error("bit extract range [" + std::to_string(from + length - 1) + ":" + std::to_string(from) + "] is out of bounds from source value ["
				+ std::to_string(source_value_.type().width()) + ":0]");
		}

		source_value_.add_target(this);
	}

	port &source_value() const { return source_value_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_bit_extract_node(*this);
	}

private:
	port &source_value_;
	int from_, length_;
};

class bit_insert_node : public value_node {
public:
	bit_insert_node(port &value, port &bits, int to, int length)
		: value_node(node_kinds::bit_insert, value.type())
		, source_value_(value)
		, bits_(bits)
		, to_(to)
		, length_(length)
	{
		if (bits.type().width() > value.type().width()) {
			throw std::runtime_error("width of type of incoming bits cannot be greater than type of value");
		}

		if (to + length - 1 > source_value_.type().width() - 1) {
			throw std::logic_error("bit insert range [" + std::to_string(to + length - 1) + ":" + std::to_string(to) + "] is out of bounds in target value ["
				+ std::to_string(source_value_.type().width()) + ":0]");
		}

		value.add_target(this);
	}

	port &source_value() const { return source_value_; }

	port &bits() const { return bits_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_bit_insert_node(*this);
	}

private:
	port &source_value_;
	port &bits_;
	int to_, length_;
};

class vector_node : public value_node {
public:
	vector_node(node_kinds kind, const value_type &type, port &vct)
		: value_node(kind, type)
		, vct_(vct)
	{
	}

	port &source_vector() const { return vct_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_vector_node(*this);
	}

private:
	port &vct_;
};

class vector_element_node : public vector_node {
public:
	vector_element_node(node_kinds kind, const value_type &type, port &vct, int index)
		: vector_node(kind, type, vct)
		, index_(index)
	{
	}

	virtual void accept(visitor &v) override
	{
		vector_node::accept(v);
		v.visit_vector_element_node(*this);
	}

private:
	int index_;
};

class vector_extract_node : public vector_element_node {
public:
	vector_extract_node(port &vct, int index)
		: vector_element_node(node_kinds::vector_extract, vct.type().element_type(), vct, index)
	{
	}

	virtual void accept(visitor &v) override
	{
		vector_element_node::accept(v);
		v.visit_vector_extract_node(*this);
	}
};

class vector_insert_node : public vector_element_node {
public:
	vector_insert_node(port &vct, int index, port &val)
		: vector_element_node(node_kinds::vector_insert, vct.type(), vct, index)
		, val_(val)
	{
	}

	virtual void accept(visitor &v) override
	{
		vector_element_node::accept(v);
		v.visit_vector_insert_node(*this);
	}

	port &insert_value() const { return val_; }

private:
	port &val_;
};
} // namespace arancini::ir
