#pragma once

#include <arancini/util/logger.h>

#include <cstdint>
#include <iostream>

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
} __attribute__((packed));

std::ostream &operator<<(std::ostream &os, const x86_cpu_state &s);

#define X86_OFFSET_OF(reg)                                                     \
    __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
} // namespace arancini::runtime::exec::x86
template <> struct fmt::formatter<arancini::runtime::exec::x86::x86_cpu_state> {
    template <typename PCTX>
    constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator
    format(const arancini::runtime::exec::x86::x86_cpu_state &regs,
           FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(),
                              "RAX:     \t{:#x}\n"
                              "RBX:     \t{:#x}\n"
                              "RCX:     \t{:#x}\n"
                              "RDX:     \t{:#x}\n"
                              "RSI:     \t{:#x}\n"
                              "RDI:     \t{:#x}\n"
                              "RBP:     \t{:#x}\n"
                              "RSP:     \t{:#x}\n"
                              "RIP:     \t{:#x}\n"
                              "R8:      \t{:#x}\n"
                              "R9:      \t{:#x}\n"
                              "R10:     \t{:#x}\n"
                              "R11:     \t{:#x}\n"
                              "R12:     \t{:#x}\n"
                              "R13:     \t{:#x}\n"
                              "R14:     \t{:#x}\n"
                              "R15:     \t{:#x}\n"
                              "flag ZF: \t{:#x}\n"
                              "flag CF: \t{:#x}\n"
                              "flag OF: \t{:#x}\n"
                              "flag SF: \t{:#x}\n"
                              "flag PF: \t{:#x}\n"
                              "flag DF: \t{:#x}",
                              regs.RAX, regs.RBX, regs.RCX, regs.RDX, regs.RSI,
                              regs.RDI, regs.RBP, regs.RSP, regs.PC, regs.R8,
                              regs.R9, regs.R10, regs.R11, regs.R12, regs.R13,
                              regs.R14, regs.R15, regs.ZF, regs.CF, regs.OF,
                              regs.SF, regs.PF, regs.DF);
    }
};
