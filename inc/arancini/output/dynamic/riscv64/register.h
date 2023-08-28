#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#pragma once

using namespace arancini::ir;
using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::riscv64;
/**
 *
 * This tags the Register with the type they represent.
 * Many instructions only work on specific bit widths, so regardless of the IR type we need to expand the types.
 * Tracking the represented type allows to only do these expansions when necessary.
 *
 */
class TypedRegister {

public:
	explicit TypedRegister(const Register &reg)
		: encoding_(reg.encoding()) {};

	explicit TypedRegister(uint32_t encoding)
		: encoding_(encoding) {};

	TypedRegister(const Register &reg1, const Register &reg2)
		: vt_ { value_type::u128() }
		, encoding1_(reg1.encoding())
		, encoding2_(reg2.encoding()) {};

	operator Register() const { return Register { encoding_ }; } // NOLINT(google-explicit-constructor)
	[[nodiscard]] const value_type &type() const { return vt_; }

	void set_type(const value_type vt) { vt_ = vt; }
	void set_actual_width(int8_t w) { actual_width_ = w; }
	void set_actual_width() { actual_width_ = (int8_t)vt_.element_width(); }

	[[nodiscard]] Register reg1() const { return Register { encoding1_ }; }
	[[nodiscard]] Register reg2() const { return Register { encoding2_ }; }

	[[nodiscard]] uint32_t encoding() const { return encoding_; }
	[[nodiscard]] uint32_t encoding1() const { return encoding1_; }
	[[nodiscard]] uint32_t encoding2() const { return encoding2_; }

	[[nodiscard]] int8_t actual_width() const
	{
		if (!actual_width_) {
			return (int8_t)vt_.element_width();
		}

		return actual_width_;
	}
	[[nodiscard]] int8_t actual_width_0() const { return actual_width_; }

	bool operator==(const TypedRegister &other) const { return encoding_ == other.encoding(); }
	bool operator!=(const TypedRegister &other) const { return encoding_ != other.encoding(); }

	bool operator==(const Register other) const { return encoding_ == other.encoding(); }
	bool operator!=(const Register other) const { return encoding_ != other.encoding(); }

	TypedRegister &operator=(const TypedRegister &) = delete;
	TypedRegister(const TypedRegister &) = delete;
	TypedRegister &operator=(TypedRegister &&) = delete;
	TypedRegister(TypedRegister &&) = delete;

private:
	/**
	 * Current type that the value in the register is accurate to. Assumes the value is represented as signed regardless of the type.
	 * Some of the bits may have resulted from sign extensions. Bits in parts of the register unused by this type may be garbage.
	 */
	value_type vt_ { value_type::u64() };

	/// Encoding of the register(s)
	union {
		uint32_t encoding_;
		struct {
			uint16_t encoding1_;
			uint16_t encoding2_;
		};
	};

	/// The actual bit width of the represented value. No bits of this come from sign extension. 0 means not overridden.
	int8_t actual_width_ { 0 };
};