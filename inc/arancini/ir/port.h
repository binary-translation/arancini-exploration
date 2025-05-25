#pragma once

#include <set>

#include <arancini/ir/value-type.h>
#include <arancini/ir/visitor.h>

namespace arancini::ir {
enum class port_kinds {
    value,
    constant,
    zero,
    negative,
    overflow,
    carry,
    operation_value
};

class value_node;

class port {
  public:
    port(port_kinds kind, const value_type &vt, value_node *owner)
        : kind_(kind), vt_(vt), owner_(owner) {}

    [[nodiscard]]
    port_kinds kind() const {
        return kind_;
    }

    [[nodiscard]]
    value_type &type() {
        return vt_;
    }

    [[nodiscard]]
    const value_type &type() const {
        return vt_;
    }

    [[nodiscard]]
    value_node *owner() {
        return owner_;
    }

    [[nodiscard]]
    const value_node *owner() const {
        return owner_;
    }

    void add_target(node *target) { targets_.insert(target); }

    std::size_t remove_target(node *target) { return targets_.erase(target); }

    [[nodiscard]]
    const std::set<node *> targets() const {
        return targets_;
    }

    void accept(visitor &v) { v.visit_port(*this); }

  private:
    port_kinds kind_;
    value_type vt_;
    value_node *owner_;
    std::set<node *> targets_;
};

} // namespace arancini::ir
