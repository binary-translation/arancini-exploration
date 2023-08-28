#pragma once
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/ir/port.h>
#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>

constexpr Register ZF = S8;
constexpr Register CF = S9;
constexpr Register OF = S10;
constexpr Register SF = S11;

constexpr Register MEM_BASE = T6;

inline void gen_constant(Assembler &assembler, int64_t imm, Register reg)
{
	auto immLo32 = (int32_t)imm;
	auto immLo12 = immLo32 << (32 - 12) >> (32 - 12); // sign extend lower 12 bit
	if (imm == immLo32) {
		int32_t imm32Hi20 = (immLo32 - immLo12);
		if (imm32Hi20 != 0) {
			assembler.lui(reg, imm32Hi20);
			if (immLo12) {
				assembler.addiw(reg, reg, immLo12);
			}
		} else {
			assembler.li(reg, imm);
		}
		return;
	} else {
		auto val = (int64_t)((uint64_t)imm - (uint64_t)(int64_t)immLo12); // Get lower 12 bits out of imm
		int shiftAmnt = 0;
		if (!Utils::IsInt(32, val)) { // Might still not fit as LUI with unsigned add
			shiftAmnt = __builtin_ctzll(val);
			val >>= shiftAmnt;
			if (shiftAmnt > 12 && !IsITypeImm(val)
				&& Utils::IsInt(32, val << 12)) { // Does not fit into 12 bits but can fit into LUI U-immediate with proper shift
				val <<= 12;
				shiftAmnt -= 12;
			}
		}

		gen_constant(assembler, val, reg);

		if (shiftAmnt) {
			assembler.slli(reg, reg, shiftAmnt);
		}

		if (immLo12) {
			assembler.addi(reg, reg, immLo12);
		}
		return;
	}
}
static inline bool is_flag_t(const value_type &type) { return type.element_width() == 1; }
static inline bool is_flag(const port &value) { return is_flag_t(value.type()); }
static inline bool is_flag_port(const port &value)
{
	return value.kind() == port_kinds::zero || value.kind() == port_kinds::carry || value.kind() == port_kinds::negative
		|| value.kind() == port_kinds::overflow;
}
static bool is_gpr_t(const value_type &type)
{
	int width = type.element_width();
	return (width == 8 || width == 16 || width == 32 || width == 64) && (!type.is_vector()) && type.is_integer();
}
static inline bool is_gpr(const port &value) { return is_gpr_t(value.type()); }
static inline bool is_gpr_or_flag(const port &value) { return (is_gpr(value) || is_flag(value)); }
static inline bool is_int(const port &value, const int w) { return value.type().element_width() == w && value.type().is_integer() && !value.type().is_vector(); }
static inline bool is_i128(const port &value) { return is_int(value, 128); }
static inline bool is_scalar_int(const port &value) { return is_gpr_or_flag(value) || is_i128(value); }
static inline bool is_int_vector_t(const value_type &type, int nr_elements, int element_width)
{
	return (type.is_vector() && type.is_integer() && type.nr_elements() == nr_elements && type.element_width() == element_width);
}
static inline bool is_int_vector(const port &value, int nr_elements, int element_width) { return is_int_vector_t(value.type(), nr_elements, element_width); }
