#pragma once

#include <fmt/core.h>

#include <vector>
#include <cstdint>

namespace arancini::ir {
enum class value_type_class : std::uint8_t { none, signed_integer, unsigned_integer, floating_point };

class value_type {
public:
	static value_type v() { return value_type(value_type_class::none, 0); }
	static value_type u1() { return value_type(value_type_class::unsigned_integer, 1, 1); }
	static value_type u8() { return value_type(value_type_class::unsigned_integer, 8, 1); }
	static value_type u16() { return value_type(value_type_class::unsigned_integer, 16, 1); }
	static value_type u32() { return value_type(value_type_class::unsigned_integer, 32, 1); }
	static value_type u64() { return value_type(value_type_class::unsigned_integer, 64, 1); }
	static value_type u128() { return value_type(value_type_class::unsigned_integer, 128, 1); }
	static value_type u256() { return value_type(value_type_class::unsigned_integer, 256, 1); }
	static value_type u512() { return value_type(value_type_class::unsigned_integer, 512, 1); }
	static value_type s8() { return value_type(value_type_class::signed_integer, 8, 1); }
	static value_type s16() { return value_type(value_type_class::signed_integer, 16, 1); }
	static value_type s32() { return value_type(value_type_class::signed_integer, 32, 1); }
	static value_type s64() { return value_type(value_type_class::signed_integer, 64, 1); }
	static value_type s128() { return value_type(value_type_class::signed_integer, 128, 1); }
	static value_type f32() { return value_type(value_type_class::floating_point, 32, 1); }
	static value_type f64() { return value_type(value_type_class::floating_point, 64, 1); }
    static value_type f80() { return value_type(value_type_class::floating_point, 80, 1); } // x87 double extended-precision

    template <typename T, typename std::enable_if<std::is_arithmetic_v<T>, int>::type = 0>
    static value_type from_value(T val) {
        if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>)
                return value_type(value_type_class::signed_integer, sizeof(T), 1);

            return value_type(value_type_class::unsigned_integer, sizeof(T), 1);
        }

        return value_type(value_type_class::floating_point, sizeof(T), 1);
    }

    using size_type = std::size_t;

	static value_type vector(const value_type &underlying_type, size_type nr_elements) {
		return value_type(underlying_type.tc_, underlying_type.element_width_, nr_elements);
	}

    value_type() = default;

	value_type(value_type_class tc, size_type element_width, size_type nr_elements = 1)
		: tc_(tc)
		, element_width_(element_width)
		, nr_elements_(nr_elements)
	{
	}

    [[nodiscard]]
	size_type element_width() const { return element_width_; }

    [[nodiscard]]
	value_type_class type_class() const { return tc_; }

    [[nodiscard]]
	size_type nr_elements() const { return nr_elements_; }

    [[nodiscard]]
	bool is_vector() const { return nr_elements_ > 1; }

    [[nodiscard]]
	size_type width() const { return element_width_ * nr_elements_; }

    [[nodiscard]]
	bool is_floating_point() const { return tc_ == value_type_class::floating_point; }

    [[nodiscard]]
	bool is_integer() const { return tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer; }

    [[nodiscard]]
	value_type element_type() const { return value_type(tc_, element_width_, 1); }

    [[nodiscard]]
	value_type get_opposite_signedness() const {
		value_type_class dst_class;

		if (tc_ == value_type_class::signed_integer)
			dst_class = value_type_class::unsigned_integer;
		else if (tc_ == value_type_class::unsigned_integer)
			dst_class = value_type_class::signed_integer;
		else
			throw std::logic_error(fmt::format("{}:{}: Initial type must be an integer", __FILE__, __LINE__));

		return value_type(dst_class, element_width_, nr_elements_);
	}

    [[nodiscard]]
	value_type get_signed_type() const {
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::signed_integer, element_width_, nr_elements_);
		throw std::logic_error(fmt::format("{}:{}: Initial type must be an integer", __FILE__, __LINE__));
	}

    [[nodiscard]]
	value_type get_unsigned_type() const {
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::unsigned_integer, element_width_, nr_elements_);
		throw std::logic_error(fmt::format("{}:{}: Initial type must be an integer", __FILE__, __LINE__));
	}
private:
	value_type_class tc_;
	size_type element_width_;
	size_type nr_elements_;
};

// Comparison operator for value type
[[nodiscard]]
inline bool operator==(const value_type &v1, const value_type &v2) {
    return v1.element_width() == v2.element_width() &&
           v1.type_class()    == v2.type_class() &&
           v1.nr_elements()   == v2.nr_elements();
}

[[nodiscard]]
inline bool operator!=(const value_type &v1, const value_type &v2) {
    return !(v1 == v2);
}

class function_type {
public:
	function_type(const value_type &return_type, const std::vector<value_type> &parameter_types)
		: return_type_(return_type)
		, param_types_(parameter_types)
	{
	}

	const value_type &return_type() const { return return_type_; }
	const std::vector<value_type> &parameter_types() const { return param_types_; }

private:
	value_type return_type_;
	std::vector<value_type> param_types_;
};

} // namespace arancini::ir

template <>
struct fmt::formatter<arancini::ir::value_type_class> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::value_type_class vt, FormatContext& ctx) const {
        using namespace arancini::ir;

		switch (vt) {
		case value_type_class::none:
            return fmt::format_to(ctx.out(), "none");
		case value_type_class::signed_integer:
            return fmt::format_to(ctx.out(), "signed integer");
		case value_type_class::unsigned_integer:
            return fmt::format_to(ctx.out(), "unsigned integer");
		case value_type_class::floating_point:
            return fmt::format_to(ctx.out(), "floating point");
		default:
            return fmt::format_to(ctx.out(), "unknown");
		}
    }
};

template <>
struct fmt::formatter<arancini::ir::value_type> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::ir::value_type vt, FormatContext& ctx) const {
        using namespace arancini::ir;

		if (vt.nr_elements() > 1) {
            fmt::format_to(ctx.out(), "v{}_", vt.nr_elements());
		}

		switch (vt.type_class()) {
		case value_type_class::none:
            return fmt::format_to(ctx.out(), "none");
		case value_type_class::signed_integer:
            return fmt::format_to(ctx.out(), "s{}", vt.element_width());
		case value_type_class::unsigned_integer:
            return fmt::format_to(ctx.out(), "u{}", vt.element_width());
		case value_type_class::floating_point:
            return fmt::format_to(ctx.out(), "f{}", vt.element_width());
		default:
            return fmt::format_to(ctx.out(), "unknown");
		}
    }
};

