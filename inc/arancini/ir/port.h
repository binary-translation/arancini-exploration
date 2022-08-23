#pragma once

#include <vector>

#include <arancini/ir/value-type.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
class visitor;

enum class port_kinds { value, constant, zero, negative, overflow, carry };

class node;
class port {
public:
	port(port_kinds kind, const value_type &vt, node *owner)
		: kind_(kind)
		, vt_(vt)
		, owner_(owner)
	{
	}

	port_kinds kind() const { return kind_; }
	const value_type &type() const { return vt_; }

	node *owner() const { return owner_; }

	bool accept(visitor &v) { return v.visit_port(*this); }

private:
	port_kinds kind_;
	value_type vt_;
	node *owner_;
};
} // namespace arancini::ir
