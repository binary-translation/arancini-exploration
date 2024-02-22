#pragma once

#include <arancini/util/logger.h>

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
    } __attribute__((packed));

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
} // namespace arancini::runtime::exec::x86
template <>
struct fmt::formatter<arancini::runtime::exec::x86::x86_cpu_state> {
    template <typename PCTX> constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator  format(const arancini::runtime::exec::x86::x86_cpu_state &regs, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "RAX:{}\n"
                                                "RBX:{}\n"
                                                "RCX:{}\n"
                                                "RDX:{}\n"
                                                "RSI:{}\n"
                                                "RDI:{}\n"
                                                "RBP:{}\n"
                                                "RSP:{}\n"
                                                "R8:{}\n"
                                                "R9:{}\n"
                                                "R10:{}\n"
                                                "R11:{}\n"
                                                "R12:{}\n"
                                                "R13:{}\n"
                                                "R14:{}\n"
                                                "R15:{}\n"
                                                "flag ZF:{}\n"
                                                "flag CF:{}\n"
                                                "flag OF:{}\n"
                                                "flag SF:{}\n"
                                                "flag PF:{}\n"
                                                "flag DF:{}",
                              regs.RAX, regs.RBX, regs.RCX,
                              regs.RDX, regs.RSI, regs.RDI,
                              regs.RBP, regs.RSP, regs.R8,
                              regs.R9, regs.R10, regs.R11,
                              regs.R12, regs.R13, regs.R14,
                              regs.R15, regs.ZF, regs.CF,
                              regs.OF, regs.SF, regs.PF,
                              regs.DF);

    }
};

