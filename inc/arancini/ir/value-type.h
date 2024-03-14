#pragma once

#include <arancini/util/static-map.h>

#include <fmt/core.h>

#include <string>
#include <vector>

namespace arancini::ir {
enum class value_type_class { none, signed_integer, unsigned_integer, floating_point };

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

	static value_type vector(const value_type &underlying_type, int nr_elements)
	{
		return value_type(underlying_type.tc_, underlying_type.element_width_, nr_elements);
	}

    value_type() = default;

	value_type(value_type_class tc, int element_width, int nr_elements = 1)
		: tc_(tc)
		, element_width_(element_width)
		, nr_elements_(nr_elements)
	{
	}

	int element_width() const { return element_width_; }
	value_type_class type_class() const { return tc_; }
	int nr_elements() const { return nr_elements_; }
	bool is_vector() const { return nr_elements_ > 1; }
	int width() const { return element_width_ * nr_elements_; }
	bool is_floating_point() const { return tc_ == value_type_class::floating_point; }
	bool is_integer() const { return tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer; }

	value_type element_type() const { return value_type(tc_, element_width_, 1); }

	bool equivalent_to(const value_type &o) const { return element_width_ == o.element_width_ && tc_ == o.tc_ && nr_elements_ == o.nr_elements_; }

	value_type get_opposite_signedness() const
	{
		value_type_class dst_class;

		if (tc_ == value_type_class::signed_integer)
			dst_class = value_type_class::unsigned_integer;
		else if (tc_ == value_type_class::unsigned_integer)
			dst_class = value_type_class::signed_integer;
		else
			throw std::logic_error(__FILE__ ":" + std::to_string(__LINE__) + ": Initial type must be an integer");

		return value_type(dst_class, element_width_, nr_elements_);
	}

	value_type get_signed_type() const
	{
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::signed_integer, element_width_, nr_elements_);
		throw std::logic_error(__FILE__ ":" + std::to_string(__LINE__) + ": Initial type must be an integer");
	}

	value_type get_unsigned_type() const
	{
		if (tc_ == value_type_class::signed_integer || tc_ == value_type_class::unsigned_integer)
			return value_type(value_type_class::unsigned_integer, element_width_, nr_elements_);
		throw std::logic_error(__FILE__ ":" + std::to_string(__LINE__) + ": Initial type must be an integer");
	}
private:
	value_type_class tc_;
	int element_width_;
	int nr_elements_;
};

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
    template <typename PCTX> constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator format(const arancini::ir::value_type &value, FCTX &format_ctx) const {
        if (value.nr_elements() > 1) 
            fmt::format_to(format_ctx.out(), "v{}", value.nr_elements());

        util::static_map<arancini::ir::value_type_class, char, 4> matches {
            { arancini::ir::value_type_class::none, 'v' },
            { arancini::ir::value_type_class::signed_integer, 's' },
            { arancini::ir::value_type_class::unsigned_integer, 'u' },
            { arancini::ir::value_type_class::floating_point, 'f' },
        };

        auto match = matches.get(value.type_class(), '?');
        return fmt::format_to(format_ctx.out(), "{}{}", match, value.element_width());
    }
};

