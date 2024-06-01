#pragma once

#include <arancini/util/logger.h>

#include <string>
#include <vector>
#include <cstdint>

namespace arancini::ir {
enum class value_type_class : std::uint8_t { none, signed_integer, unsigned_integer, floating_point };

class value_type {
public:
    using size_type = std::size_t;
    using value_type_class = arancini::ir::value_type_class;

    [[nodiscard]]
	static constexpr value_type v() { return value_type(value_type_class::none, 0); }

    [[nodiscard]]
	static constexpr value_type u1() { return value_type(value_type_class::unsigned_integer, 1, 1); }

    [[nodiscard]]
	static constexpr value_type u8() { return value_type(value_type_class::unsigned_integer, 8, 1); }

    [[nodiscard]]
	static constexpr value_type u16() { return value_type(value_type_class::unsigned_integer, 16, 1); }

    [[nodiscard]]
	static constexpr value_type u32() { return value_type(value_type_class::unsigned_integer, 32, 1); }

    [[nodiscard]]
	static constexpr value_type u64() { return value_type(value_type_class::unsigned_integer, 64, 1); }

    [[nodiscard]]
	static constexpr value_type u128() { return value_type(value_type_class::unsigned_integer, 128, 1); }

    [[nodiscard]]
	static constexpr value_type u256() { return value_type(value_type_class::unsigned_integer, 256, 1); }

    [[nodiscard]]
	static constexpr value_type u512() { return value_type(value_type_class::unsigned_integer, 512, 1); }

    [[nodiscard]]
	static constexpr value_type s8() { return value_type(value_type_class::signed_integer, 8, 1); }

    [[nodiscard]]
	static constexpr value_type s16() { return value_type(value_type_class::signed_integer, 16, 1); }

    [[nodiscard]]
	static constexpr value_type s32() { return value_type(value_type_class::signed_integer, 32, 1); }

    [[nodiscard]]
	static constexpr value_type s64() { return value_type(value_type_class::signed_integer, 64, 1); }

    [[nodiscard]]
	static constexpr value_type s128() { return value_type(value_type_class::signed_integer, 128, 1); }

    [[nodiscard]]
	static constexpr value_type f32() { return value_type(value_type_class::floating_point, 32, 1); }

    [[nodiscard]]
	static constexpr value_type f64() { return value_type(value_type_class::floating_point, 64, 1); }

    [[nodiscard]]
    static constexpr value_type f80() { return value_type(value_type_class::floating_point, 80, 1); } // x87 double extended-precision

    [[nodiscard]]
	static value_type vector(const value_type &underlying_type, int nr_elements) {
		return value_type(underlying_type.tc_, underlying_type.element_width_, nr_elements);
	}

    value_type() = default;

    [[nodiscard]]
	constexpr value_type(value_type_class tc, int element_width, int nr_elements = 1)
		: tc_(tc)
		, element_width_(element_width)
		, nr_elements_(nr_elements)
	{
	}

    [[nodiscard]]
	constexpr size_type element_width() const { return element_width_; }

    [[nodiscard]]
	constexpr value_type_class type_class() const { return tc_; }

    [[nodiscard]]
	constexpr size_type nr_elements() const { return nr_elements_; }

    [[nodiscard]]
	constexpr size_type width() const { return element_width_ * nr_elements_; }

    [[nodiscard]]
	constexpr bool is_vector() const { return nr_elements_ > 1; }

    [[nodiscard]]
	constexpr bool is_floating_point() const { return tc_ == value_type_class::floating_point; }

    [[nodiscard]]
	constexpr bool is_integer() const {
        return tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer;
    }

    [[nodiscard]]
	constexpr value_type element_type() const {
        return value_type(tc_, element_width_, 1);
    }

    [[nodiscard]]
	constexpr value_type get_opposite_signedness() const {
		value_type_class dst_class = value_type_class::none;

		if (tc_ == value_type_class::signed_integer)
			dst_class = value_type_class::unsigned_integer;
		else if (tc_ == value_type_class::unsigned_integer)
			dst_class = value_type_class::signed_integer;
		else
			throw std::logic_error(fmt::format("{}: {}: Initial type must be an integer",
                                   __FILE__, __LINE__));

		return value_type(dst_class, element_width_, nr_elements_);
	}

    [[nodiscard]]
	constexpr value_type get_signed_type() const {
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::signed_integer, element_width_, nr_elements_);
        throw std::logic_error(fmt::format("{}: {}: Initial type must be an integer",
                               __FILE__, __LINE__));
	}

    [[nodiscard]]
	constexpr value_type get_unsigned_type() const {
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::unsigned_integer, element_width_, nr_elements_);
        throw std::logic_error(fmt::format("{}: {}: Initial type must be an integer",
                               __FILE__, __LINE__));
	}
private:
	value_type_class tc_;
    size_type element_width_;
    size_type nr_elements_;
};

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
struct fmt::formatter<arancini::ir::value_type> {
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const arancini::ir::value_type& type, FormatContext& ctx) const {
        char type_designator;
		switch (type.type_class()) {
		case arancini::ir::value_type_class::none:
			type_designator = 'x';
			break;
		case arancini::ir::value_type_class::signed_integer:
			type_designator = 's';
			break;
		case arancini::ir::value_type_class::unsigned_integer:
			type_designator = 'u';
			break;
		case arancini::ir::value_type_class::floating_point:
			type_designator = 'f';
			break;
		default:
            type_designator = '?';
			break;
		}

		if (type.nr_elements() > 1) {
            return fmt::format_to(ctx.out(), "v{}_{}{}", type.nr_elements(),
                                  type.element_width(), type_designator);
        }

        return fmt::format_to(ctx.out(), "{}{}", type.element_width(), type_designator);
	}
};

