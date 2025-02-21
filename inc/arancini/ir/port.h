#pragma once


#include <arancini/ir/value-type.h>
#include <arancini/ir/visitor.h>

#include <fmt/format.h>

#include <set>

namespace arancini::ir {
enum class port_kinds { value, constant, zero, negative, overflow, carry, operation_value };

class value_node;

class port {
public:
	port(port_kinds kind, const value_type &vt, value_node *owner)
		: kind_(kind)
		, vt_(vt)
		, owner_(owner)
	{
	}

    [[nodiscard]]
	port_kinds kind() const { return kind_; }

    [[nodiscard]]
	value_type &type() { return vt_; }

    [[nodiscard]]
	const value_type &type() const { return vt_; }

    [[nodiscard]]
	value_node *owner() { return owner_; }

    [[nodiscard]]
	const value_node *owner() const { return owner_; }

	void add_target(node *target) { targets_.insert(target); }

    std::size_t remove_target(node *target) {
        return targets_.erase(target);
    }

    [[nodiscard]]
	const std::set<node *> targets() const { return targets_; }

	void accept(visitor &v) { v.visit_port(*this); }
private:
	port_kinds kind_;
	value_type vt_;
	value_node *owner_;
	std::set<node *> targets_;
};

} // namespace arancini::ir

template <>
struct fmt::formatter<arancini::ir::port_kinds> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::port_kinds kind, FormatContext& ctx) const {
        switch (kind) {
        case arancini::ir::port_kinds::value:
            return fmt::format_to(ctx.out(), "value port");
        case arancini::ir::port_kinds::constant:
            return fmt::format_to(ctx.out(), "constant port");
        case arancini::ir::port_kinds::zero:
            return fmt::format_to(ctx.out(), "zero flag port");
        case arancini::ir::port_kinds::negative:
            return fmt::format_to(ctx.out(), "negative flag port");
        case arancini::ir::port_kinds::overflow:
            return fmt::format_to(ctx.out(), "overflow flag port");
        case arancini::ir::port_kinds::carry:
            return fmt::format_to(ctx.out(), "carry flag port");
        case arancini::ir::port_kinds::operation_value:
            return fmt::format_to(ctx.out(), "operation value port");
        default:
            return fmt::format_to(ctx.out(), "unknown port value");
        }
    }
};

template <>
struct fmt::formatter<arancini::ir::port> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const arancini::ir::port& port, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{} {} {}", port.kind(), fmt::ptr(port.owner()),
                              fmt::join(port.targets(), " "));
    }
};

