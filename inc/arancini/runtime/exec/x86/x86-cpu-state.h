#pragma once

#include <arancini/util/logger.h>

#include <cstdint>
#include <iostream>

namespace arancini::runtime::exec::x86 {

struct uint128_t {
    std::uint64_t low, high;
};

struct uint256_t {
    uint128_t low, high;
};

struct uint512_t {
    uint256_t low, high;
};

struct x86_cpu_state {
#define DEFREG(ctype, ltype, name) ctype name;
#include <arancini/input/x86/reg.def>
#undef DEFREG
} __attribute__((packed));

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)

} // namespace arancini::runtime::exec::x86

template <typename T>
struct is_wide_register final :
    public std::disjunction<std::is_same<T, arancini::runtime::exec::x86::uint128_t>,
                            std::is_same<T, arancini::runtime::exec::x86::uint256_t>,
                            std::is_same<T, arancini::runtime::exec::x86::uint512_t>>
{ };

template <typename T>
constexpr bool is_wide_register_v = is_wide_register<T>::value;

template <typename Uint>
struct fmt::formatter<Uint, std::enable_if_t<is_wide_register_v<Uint>, char>> {
    template <typename PCTX>
    constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator  format(const Uint &reg, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{}_{}", reg.low, reg.high);
    }
};

template <>
struct fmt::formatter<arancini::runtime::exec::x86::x86_cpu_state> {
    template <typename PCTX>
    constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator  format(const arancini::runtime::exec::x86::x86_cpu_state &regs, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "RAX:     \t{:#x}\n"
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
                                                "flag DF: \t{:#x}\n"
                                                "ZMM0:    \t{}\n"
                                                "ZMM1:    \t{}\n"
                                                "ZMM2:    \t{}\n"
                                                "ZMM3:    \t{}\n"
                                                "ZMM4:    \t{}\n"
                                                "ZMM5:    \t{}\n"
                                                "ZMM6:    \t{}\n"
                                                "ZMM7:    \t{}\n"
                                                "ZMM8:    \t{}\n"
                                                "ZMM9:    \t{}\n"
                                                "ZMM10:   \t{}\n"
                                                "ZMM11:   \t{}\n"
                                                "ZMM12:   \t{}\n"
                                                "ZMM13:   \t{}\n"
                                                "ZMM14:   \t{}\n"
                                                "ZMM15:   \t{}\n"
                                                "ZMM16:   \t{}\n"
                                                "ZMM17:   \t{}\n"
                                                "ZMM18:   \t{}\n"
                                                "ZMM19:   \t{}\n"
                                                "ZMM20:   \t{}\n"
                                                "ZMM21:   \t{}\n"
                                                "ZMM22:   \t{}\n"
                                                "ZMM23:   \t{}\n"
                                                "ZMM24:   \t{}\n"
                                                "ZMM25:   \t{}\n"
                                                "ZMM26:   \t{}\n"
                                                "ZMM27:   \t{}\n"
                                                "ZMM28:   \t{}\n"
                                                "ZMM29:   \t{}\n"
                                                "ZMM30:   \t{}\n"
                                                "ZMM31:   \t{}",
                              regs.RAX, regs.RBX, regs.RCX,
                              regs.RDX, regs.RSI, regs.RDI,
                              regs.RBP, regs.RSP, regs.PC,
                              regs.R8, regs.R9, regs.R10,
                              regs.R11, regs.R12, regs.R13,
                              regs.R14, regs.R15, regs.ZF,
                              regs.CF, regs.OF, regs.SF,
                              regs.PF, regs.DF,
                              regs.ZMM0, regs.ZMM1, regs.ZMM2,
                              regs.ZMM3, regs.ZMM4, regs.ZMM5,
                              regs.ZMM6, regs.ZMM7, regs.ZMM8,
                              regs.ZMM9, regs.ZMM10, regs.ZMM11,
                              regs.ZMM12, regs.ZMM13, regs.ZMM14,
                              regs.ZMM15, regs.ZMM16, regs.ZMM17,
                              regs.ZMM18, regs.ZMM19, regs.ZMM20,
                              regs.ZMM21, regs.ZMM22, regs.ZMM23,
                              regs.ZMM24, regs.ZMM25, regs.ZMM26,
                              regs.ZMM27, regs.ZMM28, regs.ZMM29,
                              regs.ZMM30, regs.ZMM31);

    }
};

