#pragma once

#include <iomanip>
#include <sstream>
#include <string>

namespace arancini::ir {
enum class value_type_class { none, signed_integer, unsigned_integer, floating_point };

class value_type {
public:
	static value_type v() { return value_type(value_type_class::none, 0); }
	static value_type u1() { return value_type(value_type_class::unsigned_integer, 1); }
	static value_type u8() { return value_type(value_type_class::unsigned_integer, 8); }
	static value_type u16() { return value_type(value_type_class::unsigned_integer, 16); }
	static value_type u32() { return value_type(value_type_class::unsigned_integer, 32); }
	static value_type u64() { return value_type(value_type_class::unsigned_integer, 64); }
	static value_type u128() { return value_type(value_type_class::unsigned_integer, 128); }
	static value_type s8() { return value_type(value_type_class::signed_integer, 8); }
	static value_type s16() { return value_type(value_type_class::signed_integer, 16); }
	static value_type s32() { return value_type(value_type_class::signed_integer, 32); }
	static value_type s64() { return value_type(value_type_class::signed_integer, 64); }

	value_type(value_type_class tc, int width)
		: tc_(tc)
		, width_(width)
	{
	}

	int width() const { return width_; }
	value_type_class type_class() const { return tc_; }

	bool equivalent_to(const value_type &o) const { return width_ == o.width_ && tc_ == o.tc_; }

	std::string to_string() const
	{
		std::stringstream s;

		switch (tc_) {
		case value_type_class::none:
			return "v";

		case value_type_class::signed_integer:
			s << "s";
			break;

		case value_type_class::unsigned_integer:
			s << "u";
			break;
		case value_type_class::floating_point:
			s << "f";
			break;
		default:
			s << "?";
			break;
		}

		s << std::dec << std::to_string(width_);

		return s.str();
	}

private:
	value_type_class tc_;
	int width_;
};
} // namespace arancini::ir
