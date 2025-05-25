#pragma once
#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <unordered_map>

namespace arancini::input::x86 {
enum class reg_offsets : unsigned long {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static constexpr unsigned long counter_base_ = __COUNTER__;
enum class reg_idx : unsigned long {
#define DEFREG(ctype, ltype, name) name = __COUNTER__ - counter_base_ - 1,
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static constexpr unsigned long counter_base1_ = __COUNTER__;
static const std::unordered_map<unsigned long, unsigned long> off_to_idx{
#define DEFREG(ctype, ltype, name)                                             \
    {X86_OFFSET_OF(name), __COUNTER__ - counter_base1_ - 1},
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static const std::unordered_map<unsigned long, const char *> off_to_name{
#define DEFREG(ctype, ltype, name) {X86_OFFSET_OF(name), #name},
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static const char *regnames[] = {
#define DEFREG(ctype, ltype, name) "" #name,
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static unsigned long offset_to_idx(reg_offsets reg) {
    return arancini::input::x86::off_to_idx.at((unsigned long)reg);
}

static const char *offset_to_name(reg_offsets reg) {
    return arancini::input::x86::off_to_name.at((unsigned long)reg);
}

static std::string idx_to_reg_name(int regidx) {
    if ((size_t)regidx < (sizeof(regnames) / sizeof(regnames[0]))) {
        return regnames[regidx];
    }

    return "guestreg";
}
} // namespace arancini::input::x86
