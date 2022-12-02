#pragma once

#include <cstdint>

namespace arancini::runtime::exec::x86 {
struct uint128_t {
	uint64_t low, high;
};

struct x86_cpu_state {
#define DEFREG(idx, ctype, ltype, name) ctype name;
#include <arancini/input/x86/reg.def>
#undef DEFREG
} __attribute__((packed));

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
} // namespace arancini::runtime::exec::x86
