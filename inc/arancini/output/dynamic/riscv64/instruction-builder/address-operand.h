#pragma once

#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>
#include <arancini/output/dynamic/riscv64/instruction-builder/register-operand.h>

#include <cstdint>

namespace arancini::output::dynamic::riscv64::builder {
class AddressOperand {
public:
	explicit constexpr AddressOperand(uint32_t base_encoding, intptr_t offset)
		: base_(base_encoding)
		, offset_(offset)
	{
	}
	explicit constexpr AddressOperand(RegisterOperand base, intptr_t offset)
		: base_(base)
		, offset_(offset)
	{
	}
	explicit constexpr AddressOperand(RegisterOperand base)
		: base_(base)
		, offset_(0)
	{
	}
	explicit constexpr AddressOperand(Register base, intptr_t offset)
		: base_(base)
		, offset_(offset)
	{
	}
	explicit constexpr AddressOperand(Register base)
		: base_(base)
		, offset_(0)
	{
	}
	AddressOperand(Address address)
		: base_(address.base())
		, offset_(address.offset())
	{
	}
	[[nodiscard]] bool is_virtual() const { return base_.is_virtual(); }
	[[nodiscard]] bool is_physical() const { return base_.is_physical(); }

	operator Address() const { return Address { base_, offset_ }; }
	[[nodiscard]] RegisterOperand base() const { return base_; }
	[[nodiscard]] intptr_t offset() const { return offset_; }

private:
	RegisterOperand base_;
	intptr_t offset_;
};

} // namespace arancini::output::dynamic::riscv64::builder