#pragma once

#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>
#include <arancini/ir/metadata.h>

#include <fmt/core.h>

#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace arancini::ir {

enum class node_kinds {
	label = 0,
	read_pc,
	write_pc,
	constant,
	unary_arith,
	binary_arith,
	ternary_arith,
	unary_atomic,
	binary_atomic,
	ternary_atomic,
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
	vector_insert,
	read_local,
	write_local,
	internal_call
};

enum br_type { none,sys , br, csel, call, ret };

class ir_exception : public std::runtime_error {
public:
    template <typename... Args>
    ir_exception(std::string_view format, Args&&... args):
        std::runtime_error(fmt::format(format, std::forward<Args>(args)...))
    { }
};

class node {
public:
	node(node_kinds kind)
		: kind_(kind)
	{ }

    [[nodiscard]]
	node_kinds kind() const { return kind_; }

    [[nodiscard]]
	virtual bool is_action() const { return false; }

	virtual void accept(visitor &v) { v.visit_node(*this); }
#ifndef NDEBUG
	void set_metadata(const std::string &key, std::shared_ptr<metadata> value) {
        md_[key] = value;
    }

    [[nodiscard]]
	std::shared_ptr<metadata> get_metadata(const std::string &key) const { return md_.at(key); }

    [[nodiscard]]
	std::vector<std::pair<std::string, std::shared_ptr<metadata>>> get_metadata_of_kind(metadata_kind kind) const {
		std::vector<std::pair<std::string, std::shared_ptr<metadata>>> r;

		for (auto &n : md_) {
			if (n.second->kind() == kind) {
				r.push_back({ n.first, n.second });
			}
		}

		return r;
	}

    [[nodiscard]]
	std::shared_ptr<metadata> try_get_metadata(const std::string &key) const {
		auto m = md_.find(key);

		if (m == md_.end()) {
			return nullptr;
		} else {
			return m->second;
		}
	}

    [[nodiscard]]
	bool has_metadata(const std::string &key) const { return md_.count(key) > 0; }
#endif

	virtual ~node() = default;
private:
	node_kinds kind_;
#ifndef NDEBUG
	std::unordered_map<std::string, std::shared_ptr<metadata>> md_;
#endif
};


class value_node : public node {
public:
	value_node(node_kinds kind, const value_type &vt)
		: node(kind)
		, value_(port_kinds::value, vt, this)
	{
	}

    [[nodiscard]]
	port &val() { return value_; }

    [[nodiscard]]
	const port &val() const { return value_; }

	virtual void accept(visitor &v) override {
		node::accept(v);
		v.visit_value_node(*this);
	}
protected:
	port value_;
};

class action_node : public value_node {
public:
	action_node(node_kinds kind)
		: value_node(kind, value_type(value_type_class::none, 0, 0))
	{ }

	action_node(node_kinds kind, const value_type &vt)
		: value_node(kind, vt)
	{ }

    [[nodiscard]]
	virtual bool is_action() const override { return true; }

    [[nodiscard]]
	virtual br_type updates_pc() const { return br_type::none; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		node::accept(v);
		v.visit_action_node(*this);
	}
};

class label_node : public action_node {
public:
	label_node(std::string name)
		: action_node(node_kinds::label)
		, name_(name)
	{ }

    // TODO: this looks like a mistake
	label_node()
		: label_node("")
	{ }

    [[nodiscard]]
	const std::string& name() const { return name_; }

	virtual void accept(visitor &v) override {
		action_node::accept(v);
		v.visit_label_node(*this);
	}

	void add_use() {
		if (used_) {
			throw ir_exception("Label node (name: {}) used by multiple jumps", name_);
		}
		used_ = true;
	}
private:
	bool used_{false};

	std::string name_;
};

class br_node : public action_node {
public:
	br_node(label_node *target)
		: action_node(node_kinds::br)
		, target_(target)
	{
		if (target) {
			target->add_use();
		}
	}

    [[nodiscard]]
	label_node *target() { return target_; }

    [[nodiscard]]
	const label_node *target() const { return target_; }

	void add_br_target(label_node *n) {
		target_ = n;
		n->add_use();
	}

	virtual void accept(visitor &v) override {
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
		if (target) {
			target->add_use();
		}
	}

    [[nodiscard]]
	port &cond() { return cond_; }

    [[nodiscard]]
	const port &cond() const { return cond_; }

    [[nodiscard]]
	label_node *target() { return target_; }

    [[nodiscard]]
	const label_node *target() const { return target_; }

	void add_br_target(label_node *n) {
		target_ = n;
		n->add_use();

	}

	virtual void accept(visitor &v) override {
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
	{ }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_read_pc_node(*this);
	}
};

class write_pc_node : public action_node {
public:
	write_pc_node(port &value, br_type br_type, unsigned long target)
		: action_node(node_kinds::write_pc)
		, value_(value)
		, br_type_(br_type)
		, target_(target)
	{
		value.add_target(this);
	}

    [[nodiscard]]
	port &value() { return value_; }

    [[nodiscard]]
	const port &value() const { return value_; }

    [[nodiscard]]
	virtual br_type updates_pc() const override { return br_type_; }

	virtual void accept(visitor &v) override {
		action_node::accept(v);
		v.visit_write_pc_node(*this);
	}

    [[nodiscard]]
	unsigned long const_target() const { return target_; };
private:
	port &value_;
	br_type br_type_;
	unsigned long target_;
};

class constant_node : public value_node {
public:
	constant_node(const value_type &vt, unsigned long cv)
		: value_node(node_kinds::constant, vt)
		, cvi_(cv)
	{
		if (vt.type_class() != value_type_class::signed_integer && vt.type_class() != value_type_class::unsigned_integer) {
			throw ir_exception("constructing a constant node with an integer (value: {}) for a non-integer value type {}",
                               cv, vt);
		}
	}

	constant_node(const value_type &vt, double cv)
		: value_node(node_kinds::constant, vt)
		, cvf_(cv)
	{
		if (vt.type_class() != value_type_class::floating_point) {
			throw ir_exception("constructing a constant node with a float {} for a non-float value type {}",
                               cv, vt);
		}
	}

    [[nodiscard]]
	unsigned long const_val_i() const { return cvi_; }

    [[nodiscard]]
	double const_val_f() const { return cvf_; }

    [[nodiscard]]
	bool is_zero() const { return val().type().is_floating_point() ? cvf_ == 0 : cvi_ == 0; }

	virtual void accept(visitor &v) override {
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
	read_reg_node(const value_type &vt, unsigned long regoff, unsigned long regidx, const char *regname)
		: value_node(node_kinds::read_reg, vt)
		, regoff_(regoff)
		, regidx_(regidx)
		, regname_(regname)
	{
	}

    [[nodiscard]]
	unsigned long regoff() const { return regoff_; }

    [[nodiscard]]
	unsigned long regidx() const { return regidx_; }

    [[nodiscard]]
	const char *regname() const { return regname_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_read_reg_node(*this);
	}
private:
	unsigned long regoff_;
	unsigned long regidx_;
	const char *regname_;
};

class read_mem_node : public value_node {
public:
	read_mem_node(const value_type &vt, port &addr)
		: value_node(node_kinds::read_mem, vt)
		, addr_(addr)
	{
		addr.add_target(this);
	}

    [[nodiscard]]
	port &address() { return addr_; }

    [[nodiscard]]
	const port &address() const { return addr_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_read_mem_node(*this);
	}
private:
	port &addr_;
};

class write_reg_node : public action_node {
public:
	write_reg_node(unsigned long regoff, unsigned long regidx, const char *regname, port &val)
		: action_node(node_kinds::write_reg)
		, regoff_(regoff)
		, regidx_(regidx)
		, regname_(regname)
		, val_(val)
	{
		val.add_target(this);
	}

    [[nodiscard]]
	unsigned long regoff() const { return regoff_; }

    [[nodiscard]]
	unsigned long regidx() const { return regidx_; }

    [[nodiscard]]
	const char *regname() const { return regname_; }

    [[nodiscard]]
	port &value() { return val_; }

    [[nodiscard]]
	const port &value() const { return val_; }

	virtual void accept(visitor &v) override {
		action_node::accept(v);
		v.visit_write_reg_node(*this);
	}
private:
	unsigned long regoff_;
	unsigned long regidx_;
	const char *regname_;
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

    [[nodiscard]]
	port &address() { return addr_; }

    [[nodiscard]]
	const port &address() const { return addr_; }

    [[nodiscard]]
	port &value() { return val_; }

    [[nodiscard]]
	const port &value() const { return val_; }

	virtual void accept(visitor &v) override {
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

    [[nodiscard]]
	port &condition() { return condition_; }

    [[nodiscard]]
	const port &condition() const { return condition_; }

    [[nodiscard]]
	port &trueval() { return trueval_; }

    [[nodiscard]]
	const port &trueval() const { return trueval_; }

    [[nodiscard]]
	port &falseval() { return falseval_; }

    [[nodiscard]]
	const port &falseval() const { return falseval_; }

	virtual void accept(visitor &v) override {
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
		, zero_(port_kinds::zero, value_type::u1(), this)
		, negative_(port_kinds::negative, value_type::u1(), this)
	{
		input.add_target(this);
		amount.add_target(this);
	}

    [[nodiscard]]
	shift_op op() const { return op_; }

    [[nodiscard]]
	port &input() { return input_; }

    [[nodiscard]]
	const port &input() const { return input_; }

    [[nodiscard]]
	port &amount() { return amount_; }

    [[nodiscard]]
	const port &amount() const { return amount_; }

    [[nodiscard]]
	port &zero() { return zero_; }

    [[nodiscard]]
	const port &zero() const { return zero_; }

    [[nodiscard]]
	port &negative() { return negative_; }

    [[nodiscard]]
	const port &negative() const { return negative_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_bit_shift_node(*this);
	}
private:
	shift_op op_;
	port &input_;
	port &amount_;
	port zero_, negative_;
};

enum class cast_op : uint8_t { bitcast, zx, sx, trunc, convert };
enum class fp_convert_type : uint8_t { none, round, trunc };

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
            [[unlikely]]
			if (target_type.width() != source_value.type().width())
				throw ir_exception("cannot bitcast from {} to {} because their classes differ",
                                   source_value.type(), target_type);
		} else if (op == cast_op::convert) {
			if ((target_type.type_class() != value_type_class::floating_point) &&
                (source_value.type().type_class() != value_type_class::floating_point))
            {
                [[unlikely]]
				if (target_type.type_class() == source_value.type().type_class())
					throw ir_exception("cannot convert from {} to {} because they are the same",
                                       source_value.type(), target_type);
			}
		} else if (op != cast_op::zx) {
            [[unlikely]]
			if (target_type.type_class() != source_value.type().type_class())
				throw ir_exception("cannot perform non-sign-extend cast from {} to {} because type classes differ",
                                   source_value.type(), target_type);
		}

        [[unlikely]]
		if ((convert_type != fp_convert_type::none) && (op != cast_op::convert)) {
			throw ir_exception("conversion type {} not supported for non-convert operations in casting from {} to {}",
                               convert_type, source_value.type(), target_type);
		}

		source_value.add_target(this);
	}

	cast_node(cast_op op, const value_type &target_type, port &source_value)
		: cast_node(op, target_type, source_value, fp_convert_type::none)
	{ }

    [[nodiscard]]
	cast_op op() const { return op_; }

    [[nodiscard]]
	fp_convert_type convert_type() const { return convert_type_; }

    [[nodiscard]]
	port &source_value() { return source_value_; }

    [[nodiscard]]
	const port &source_value() const { return source_value_; }

    [[nodiscard]]
	value_type &target_type() { return target_type_; }

    [[nodiscard]]
	const value_type &target_type() const { return target_type_; }

	virtual void accept(visitor &v) override {
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

    [[nodiscard]]
	port &zero() { return zero_; }

    [[nodiscard]]
	port &negative() { return negative_; }

    [[nodiscard]]
	port &overflow() { return overflow_; }

    [[nodiscard]]
	port &carry() { return carry_; }

    [[nodiscard]]
	const port &zero() const { return zero_; }

    [[nodiscard]]
	const port &negative() const { return negative_; }

    [[nodiscard]]
	const port &overflow() const { return overflow_; }

    [[nodiscard]]
	const port &carry() const { return carry_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_arith_node(*this);
	}
private:
	port zero_, negative_, overflow_, carry_;
};

enum class unary_arith_op { bnot, neg, complement, sqrt };

class unary_arith_node : public arith_node {
public:
	unary_arith_node(unary_arith_op op, port &lhs)
		: arith_node(node_kinds::unary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
	{
		lhs.add_target(this);
	}

    [[nodiscard]]
	unary_arith_op op() const { return op_; }

    [[nodiscard]]
	port &lhs() { return lhs_; }

    [[nodiscard]]
	const port &lhs() const { return lhs_; }

	virtual void accept(visitor &v) override {
		arith_node::accept(v);
		v.visit_unary_arith_node(*this);
	}
private:
	unary_arith_op op_;
	port &lhs_;
};

enum class binary_arith_op : std::uint8_t {
	add, sub, mul, div, mod,
	band, bor, bxor,
	cmpeq, cmpne, cmpgt,
	// floating point stuff
	cmpoeq, cmpolt, cmpole,
	cmpueq, cmpult, cmpune, cmpunlt, cmpunle,
	cmpo, cmpu
};

class binary_arith_node : public arith_node {
public:
	binary_arith_node(binary_arith_op op, port &lhs, port &rhs)
		: arith_node(node_kinds::binary_arith, lhs.type())
		, op_(op)
		, lhs_(lhs)
		, rhs_(rhs)
	{
		switch (op) {
			case binary_arith_op::band:
			case binary_arith_op::bor:
			case binary_arith_op::bxor: {
				if (lhs.type().width() != rhs.type().width()) {
					throw ir_exception("incompatible types in binary arith node: lhs={}, rhs={}",
									   lhs.type(), rhs.type());
				}
			} break;
		default: break;
		}
		op_ = op;
		lhs_ = lhs;
		rhs_ = rhs;

		lhs.add_target(this);
		rhs.add_target(this);
	}

    [[nodiscard]]
	binary_arith_op op() const { return op_; }

    [[nodiscard]]
	port &lhs() { return lhs_; }

    [[nodiscard]]
	const port &lhs() const { return lhs_; }

    [[nodiscard]]
	port &rhs() { return rhs_; }

    [[nodiscard]]
	const port &rhs() const { return rhs_; }

	virtual void accept(visitor &v) override {
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

    [[nodiscard]]
	port &lhs() { return lhs_; }

    [[nodiscard]]
	const port &lhs() const { return lhs_; }

    [[nodiscard]]
	port &rhs() { return rhs_; }

    [[nodiscard]]
	const port &rhs() const { return rhs_; }

    [[nodiscard]]
	port &top() { return top_; }

    [[nodiscard]]
	const port &top() const { return top_; }

	virtual void accept(visitor &v) override {
		arith_node::accept(v);
		v.visit_ternary_arith_node(*this);
	}
private:
	ternary_arith_op op_;
	port &lhs_;
	port &rhs_;
	port &top_;
};

class atomic_node : public action_node {
public:
	atomic_node(node_kinds kind, value_type vt)
		: action_node(kind, vt)
		, zero_(port_kinds::zero, value_type::u1(), this)
		, negative_(port_kinds::negative, value_type::u1(), this)
		, overflow_(port_kinds::overflow, value_type::u1(), this)
		, carry_(port_kinds::carry, value_type::u1(), this)
		, operation_value_(port_kinds::operation_value, vt, this)
	{ }

    [[nodiscard]]
	port &value() { return value_; }

    [[nodiscard]]
	port &zero() { return zero_; }

    [[nodiscard]]
	port &negative() { return negative_; }

    [[nodiscard]]
	port &overflow() { return overflow_; }

    [[nodiscard]]
	port &carry() { return carry_; }

    [[nodiscard]]
	port &operation_value() { return operation_value_; }

    [[nodiscard]]
	const port &zero() const { return zero_; }

    [[nodiscard]]
	const port &negative() const { return negative_; }

    [[nodiscard]]
	const port &overflow() const { return overflow_; }

    [[nodiscard]]
	const port &carry() const { return carry_; }

    [[nodiscard]]
	const port &operation_value() const { return operation_value_; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		action_node::accept(v);
		v.visit_atomic_node(*this);
	}
private:
	port zero_, negative_, overflow_, carry_, operation_value_;
};

enum class unary_atomic_op { neg, bnot };
class unary_atomic_node : public atomic_node {
public:
	unary_atomic_node(unary_atomic_op op, port &lhs)
		: atomic_node(node_kinds::unary_atomic, lhs.type())
		, op_(op)
		, lhs_(lhs)
	{
		lhs.add_target(this);
	}

    [[nodiscard]]
	unary_atomic_op op() const { return op_; }

    [[nodiscard]]
	port &lhs() { return lhs_; }

    [[nodiscard]]
	const port &lhs() const { return lhs_; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		atomic_node::accept(v);
		v.visit_unary_atomic_node(*this);
	}
private:
	unary_atomic_op op_;
	port &lhs_;
};

enum class binary_atomic_op { add, sub, band, bor, xadd, bxor, btc, btr, bts, xchg };

class binary_atomic_node : public atomic_node {
public:
	binary_atomic_node(binary_atomic_op op, port &address, port &operand)
		: atomic_node(node_kinds::binary_atomic, operand.type())
		, op_(op)
		, address_(address)
		, operand_(operand)
	{
		address.add_target(this);
		operand.add_target(this);
	}

    [[nodiscard]]
	binary_atomic_op op() const { return op_; }

    [[nodiscard]]
	port &address() { return address_; }

    [[nodiscard]]
	const port &address() const { return address_; }

    [[nodiscard]]
	port &rhs() { return operand_; }

    [[nodiscard]]
	const port &rhs() const { return operand_; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		atomic_node::accept(v);
		v.visit_binary_atomic_node(*this);
	}
private:
	binary_atomic_op op_;
	port &address_;
	port &operand_;
};

enum class ternary_atomic_op { adc, sbb, cmpxchg };

class ternary_atomic_node : public atomic_node {
public:
	ternary_atomic_node(ternary_atomic_op op, port &address, port &rhs, port &top)
		: atomic_node(node_kinds::ternary_atomic, rhs.type())
		, op_(op)
		, address_(address)
		, rhs_(rhs)
		, top_(top)
	{
		address.add_target(this);
		rhs.add_target(this);
		top.add_target(this);
	}

    [[nodiscard]]
	ternary_atomic_op op() const { return op_; }

    [[nodiscard]]
	port &address() { return address_; }

    [[nodiscard]]
	const port &address() const { return address_; }

    [[nodiscard]]
	port &rhs() { return rhs_; }

    [[nodiscard]]
	const port &rhs() const { return rhs_; }

    [[nodiscard]]
	port &top() { return top_; }

    [[nodiscard]]
	const port &top() const { return top_; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		atomic_node::accept(v);
		v.visit_ternary_atomic_node(*this);
	}
private:
	ternary_atomic_op op_;
	port &address_;
	port &rhs_;
	port &top_;
};

class bit_extract_node : public value_node {
public:
	bit_extract_node(port &value, std::size_t from, std::size_t length)
		: value_node(node_kinds::bit_extract, value_type(value_type_class::unsigned_integer, length))
		, source_value_(value)
		, from_(from)
		, length_(length)
	{
		if (from + length - 1 > source_value_.type().width() - 1) {
			throw ir_exception("bit extract range [{}:{}] is out of bound from source value [{}:0]",
                               from + length - 1, from, source_value_.type().width());
		}

		source_value_.add_target(this);
	}

    [[nodiscard]]
	port &source_value() { return source_value_; }

    [[nodiscard]]
	const port &source_value() const { return source_value_; }

    [[nodiscard]]
    std::size_t from() const { return from_; }

    [[nodiscard]]
	std::size_t length() const { return length_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_bit_extract_node(*this);
	}
private:
	port &source_value_;
    std::size_t from_, length_;
};

class bit_insert_node : public value_node {
public:
	bit_insert_node(port &value, port &bits, std::size_t to, std::size_t length)
		: value_node(node_kinds::bit_insert, value.type())
		, source_value_(value)
		, bits_(bits)
		, to_(to)
		, length_(length)
	{
        [[unlikely]]
		if (bits.type().width() > value.type().width()) {
			throw ir_exception("width of type of incoming bits cannot be greater than type of value");
		}

        [[unlikely]]
		if (length > source_value_.type().width()) {
			throw ir_exception("width of type of incoming bits cannot be smaller than requested length");
		}

        [[unlikely]]
		if (to + length - 1 > source_value_.type().width() - 1) {
			throw ir_exception("bit insert range [{}:{}] is out of bounds in target value [{}:0]",
                               to + length - 1, to, source_value_.type().width());
		}

		value.add_target(this);
	}

    [[nodiscard]]
	port &source_value() { return source_value_; }

    [[nodiscard]]
	const port &source_value() const { return source_value_; }

    [[nodiscard]]
	port &bits() { return bits_; }

    [[nodiscard]]
	const port &bits() const { return bits_; }

    [[nodiscard]]
    std::size_t to() const { return to_; }

    [[nodiscard]]
	std::size_t length() const { return length_; }

	virtual void accept(visitor &v) override
	{
		value_node::accept(v);
		v.visit_bit_insert_node(*this);
	}

private:
	port &source_value_;
	port &bits_;
    std::size_t to_, length_;
};

class vector_node : public value_node {
public:
	vector_node(node_kinds kind, const value_type &type, port &vct)
		: value_node(kind, type)
		, vct_(vct)
	{
	}

    [[nodiscard]]
	port &source_vector() { return vct_; }

    [[nodiscard]]
	const port &source_vector() const { return vct_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_vector_node(*this);
	}
private:
	port &vct_;
};

class vector_element_node : public vector_node {
public:
	vector_element_node(node_kinds kind, const value_type &type, port &vct, std::size_t index)
		: vector_node(kind, type, vct)
		, index_(index)
	{ }

    std::size_t index() const { return index_; }

	virtual void accept(visitor &v) override {
		vector_node::accept(v);
		v.visit_vector_element_node(*this);
	}
private:
	int index_;
};

class vector_extract_node : public vector_element_node {
public:
	vector_extract_node(port &vct, std::size_t index)
		: vector_element_node(node_kinds::vector_extract, vct.type().element_type(), vct, index)
	{ }

	virtual void accept(visitor &v) override {
		vector_element_node::accept(v);
		v.visit_vector_extract_node(*this);
	}
};

class vector_insert_node : public vector_element_node {
public:
	vector_insert_node(port &vct, std::size_t index, port &val)
		: vector_element_node(node_kinds::vector_insert, vct.type(), vct, index)
		, val_(val)
	{ }

	virtual void accept(visitor &v) override {
		vector_element_node::accept(v);
		v.visit_vector_insert_node(*this);
	}

    [[nodiscard]]
	port &insert_value() { return val_; }

    [[nodiscard]]
	const port &insert_value() const { return val_; }
private:
	port &val_;
};

class local_var {
public:
	local_var(const value_type &type)
		: type_(type)
	{ }

    [[nodiscard]]
	value_type &type() { return type_; }

    [[nodiscard]]
	const value_type &type() const { return type_; }
private:
	value_type type_;
};

class read_local_node : public value_node {
public:
	read_local_node(const local_var *local)
		: value_node(node_kinds::read_local, local->type())
		, lvar_(local)
	{ }

    [[nodiscard]]
	const local_var *local() const { return lvar_; }

	virtual void accept(visitor &v) override {
		value_node::accept(v);
		v.visit_read_local_node(*this);
	}
private:
	const local_var *lvar_;
};

class write_local_node : public action_node {
public:
	write_local_node(const local_var *local, port &val)
		: action_node(node_kinds::write_local)
		, lvar_(local)
		, val_(val)
	{
	}

    [[nodiscard]]
	const local_var *local() const { return lvar_; }

    [[nodiscard]]
	port &write_value() { return val_; }

    [[nodiscard]]
	const port &write_value() const { return val_; }

	virtual void accept(visitor &v) override {
		action_node::accept(v);
		v.visit_write_local_node(*this);
	}
private:
	const local_var *lvar_;
	port &val_;
};

class internal_function {
public:
	internal_function(const std::string &name, const function_type &signature)
		: name_(name)
		, sig_(signature)
	{
	}

    [[nodiscard]]
	const std::string &name() const { return name_; }

    [[nodiscard]]
	const function_type &signature() const { return sig_; }
private:
	std::string name_;
	function_type sig_;
};

class internal_call_node : public action_node {
public:
	internal_call_node(const std::shared_ptr<internal_function> &fn, const std::vector<port *> &args)
		: action_node(node_kinds::internal_call, fn->signature().return_type())
		, fn_(fn)
		, args_(args)
	{ }

    [[nodiscard]]
	const internal_function &fn() const { return *fn_; }

    [[nodiscard]]
	const std::vector<port *> &args() const { return args_; }

    [[nodiscard]]
	virtual br_type updates_pc() const override { return br_type::sys; }

	virtual void accept(visitor &v) override {
		if (v.seen_node(this))
			return;
		action_node::accept(v);
		v.visit_internal_call_node(*this);
	}
private:
	const std::shared_ptr<internal_function> fn_;
	std::vector<port *> args_;
};

} // namespace arancini::ir

// Formatters
template <>
struct fmt::formatter<arancini::ir::shift_op> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::shift_op op, FormatContext& ctx) const {
        switch (op) {
        case arancini::ir::shift_op::lsl:
            return format_to(ctx.out(), "logical shift-left");
        case arancini::ir::shift_op::lsr:
            return format_to(ctx.out(), "logical shift-right");
        case arancini::ir::shift_op::asr:
            return format_to(ctx.out(), "arithmetic shift-right");
        default:
            return format_to(ctx.out(), "unknown shift operation");
        }
    }
};

template <>
struct fmt::formatter<arancini::ir::cast_op> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::cast_op op, FormatContext& ctx) const {
        using arancini::ir::cast_op;

        switch (op) {
        case cast_op::bitcast:
            return format_to(ctx.out(), "bitcast operation");
        case cast_op::zx:
            return format_to(ctx.out(), "zero-extend operation");
        case cast_op::sx:
            return format_to(ctx.out(), "sign-extend operation");
        case cast_op::trunc:
            return format_to(ctx.out(), "truncate operation");
        case cast_op::convert:
            return format_to(ctx.out(), "convert operation");
        default:
            return format_to(ctx.out(), "unknown cast operation");
        }
    }
};

template <>
struct fmt::formatter<arancini::ir::fp_convert_type> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::fp_convert_type op, FormatContext& ctx) const {
        using arancini::ir::fp_convert_type;

        switch (op) {
        case fp_convert_type::none:
            return format_to(ctx.out(), "no floating-point conversion");
        case fp_convert_type::round:
            return format_to(ctx.out(), "floating-point rounding conversion");
        case fp_convert_type::trunc:
            return format_to(ctx.out(), "floating-point truncating conversion");
        default:
            return format_to(ctx.out(), "unknown cast operation");
        }
    }
};

template <>
struct fmt::formatter<arancini::ir::binary_arith_op> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::binary_arith_op op, FormatContext& ctx) const {
        using arancini::ir::binary_arith_op;

        switch (op) {
            case binary_arith_op::add:
                return format_to(ctx.out(), "addition binary operation");
            case binary_arith_op::sub:
                return format_to(ctx.out(), "subtraction binary operation");
            case binary_arith_op::mul:
                return format_to(ctx.out(), "multiplication binary operation");
            case binary_arith_op::div:
                return format_to(ctx.out(), "division binary operation");
            case binary_arith_op::mod:
                return format_to(ctx.out(), "modulo binary operation");
            case binary_arith_op::band:
                return format_to(ctx.out(), "logical AND binary operation");
            case binary_arith_op::bor:
                return format_to(ctx.out(), "logical OR binary operation");
            case binary_arith_op::bxor:
                return format_to(ctx.out(), "exclusive-OR binary operation");
            case binary_arith_op::cmpeq:
                return format_to(ctx.out(), "compare-equal binary operation");
            case binary_arith_op::cmpne:
                return format_to(ctx.out(), "compare-not-equal binary operation");
            case binary_arith_op::cmpgt:
                return format_to(ctx.out(), "compare-greater-than binary operation");
            case binary_arith_op::cmpoeq:
                return format_to(ctx.out(), "subtraction binary operation");
        default:
            return format_to(ctx.out(), "unknown cast operation");
        }
    }
};


template <>
struct fmt::formatter<arancini::ir::node_kinds> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::node_kinds kind, FormatContext& ctx) const {
        using arancini::ir::node_kinds;
        switch (kind) {
        case node_kinds::label:
            return fmt::format_to(ctx.out(), "label node");
        case node_kinds::read_pc:
            return fmt::format_to(ctx.out(), "read PC node");
        case node_kinds::write_pc:
            return fmt::format_to(ctx.out(), "write PC node");
        case node_kinds::constant:
            return fmt::format_to(ctx.out(), "constant node");
        case node_kinds::unary_arith:
            return fmt::format_to(ctx.out(), "unary arithmetic node");
        case node_kinds::binary_arith:
            return fmt::format_to(ctx.out(), "binary arithmetic node");
        case node_kinds::ternary_arith:
            return fmt::format_to(ctx.out(), "ternary arithmetic node");
        case node_kinds::unary_atomic:
            return fmt::format_to(ctx.out(), "unary atomic node");
        case node_kinds::binary_atomic:
            return fmt::format_to(ctx.out(), "binary atomic node");
        case node_kinds::ternary_atomic:
            return fmt::format_to(ctx.out(), "ternary atomic node");
        case node_kinds::read_reg:
            return fmt::format_to(ctx.out(), "read register node");
        case node_kinds::read_mem:
            return fmt::format_to(ctx.out(), "read memory node");
        case node_kinds::write_reg:
            return fmt::format_to(ctx.out(), "write register node");
        case node_kinds::write_mem:
            return fmt::format_to(ctx.out(), "write memory node");
        case node_kinds::cast:
            return fmt::format_to(ctx.out(), "cast node");
        case node_kinds::csel:
            return fmt::format_to(ctx.out(), "conditional select node");
        case node_kinds::bit_shift:
            return fmt::format_to(ctx.out(), "bit shift node");
        case node_kinds::br:
            return fmt::format_to(ctx.out(), "branch node");
        case node_kinds::cond_br:
            return fmt::format_to(ctx.out(), "conditional branch node");
        case node_kinds::bit_extract:
            return fmt::format_to(ctx.out(), "bit extract node");
        case node_kinds::bit_insert:
            return fmt::format_to(ctx.out(), "bit insert node");
        case node_kinds::vector_extract:
            return fmt::format_to(ctx.out(), "vector extract node");
        case node_kinds::vector_insert:
            return fmt::format_to(ctx.out(), "vector insert node");
        case node_kinds::read_local:
            return fmt::format_to(ctx.out(), "read local node");
        case node_kinds::write_local:
            return fmt::format_to(ctx.out(), "write local node");
        case node_kinds::internal_call:
            return fmt::format_to(ctx.out(), "internal call node");
        default:
            return format_to(ctx.out(), "unknown node");
        }
    }
};

