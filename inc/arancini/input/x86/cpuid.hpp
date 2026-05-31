#pragma once
#include <arancini/input/x86/cpuid.def>
#include <boost/functional/hash.hpp>
#include <stdint.h>
#include <unordered_map>

namespace arancini::input::x86 {
struct cpuid_result {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

static const std::unordered_map<std::pair<uint32_t, uint32_t>, cpuid_result *,
                                boost::hash<std::pair<uint32_t, uint32_t>>>
    cpuid_map{
#define DEF_CPUID(eax, ecx, result) {{eax, ecx}, (cpuid_result *)result},
#include <arancini/input/x86/cpuid.def>
#undef DEF_CPUID
    };

} // namespace arancini::input::x86