#pragma once

#include <cstdint>

namespace arancini::runtime::exec::x86 {
  typedef struct uint128_t {
    uint64_t low, high;
  } uint128_t;
  typedef struct uint256_t {
    uint128_t low, high;
  } uint256_t;
  typedef struct uint512_t {
    uint256_t low, high;
  } uint512_t;

	struct x86_cpu_state {
#define DEFREG(ctype, ltype, name) ctype name;
#include <arancini/input/x86/reg.def>
#undef DEFREG
    };

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
} // namespace arancini::runtime::exec::x86
