#pragma once

#include <cstdint>

namespace arancini::runtime::exec::x86 {
struct uint128_t {
	float _3, _2, _1, _0;
};

struct ymm_t {
	float _7, _6, _5, _4;
	float _3, _2, _1, _0;
};

struct zmm_t {
	float _15, _14, _13, _12;
	float _11, _10, _9, _8;
	float _7, _6, _5, _4;
	float _3, _2, _1, _0;
};

struct x86_cpu_state {
#define DEFREG(idx, ctype, ltype, name) ctype name;
#include <arancini/input/x86/reg.def>
#undef DEFREG
} __attribute__((packed));

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
} // namespace arancini::runtime::exec::x86
