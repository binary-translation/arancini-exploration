#pragma once

#include <set>
#include <vector>

#include <arancini/ir/value-type.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
enum class port_kinds { value, constant, zero, negative, overflow, carry };

class value_node;
class port {
public:
	port(port_kinds kind, const value_type &vt, value_node *owner)
		: kind_(kind)
		, vt_(vt)
		, owner_(owner)
	{
	}

	port_kinds kind() const { return kind_; }
	const value_type &type() const { return vt_; }

	value_node *owner() const { return owner_; }

	void add_target(node *target) { targets_.insert(target); }

	size_t remove_target(node *target) { return targets_.erase(target); }

	const std::set<node *> targets() const { return targets_; }

	void accept(visitor &v) { v.visit_port(*this); }

private:
	port_kinds kind_;
	value_type vt_;
	value_node *owner_;
	std::set<node *> targets_;
};
} // namespace arancini::ir
