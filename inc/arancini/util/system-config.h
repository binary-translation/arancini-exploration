#pragma once

namespace util {

#ifndef ENABLE_VERBOSE_CODE_GEN
#define ENABLE_VERBOSE_CODE_GEN false
#endif // ENABLE_VERBOSE_CODE_GEN

#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING false
#endif // ENABLE_LOG

struct system_config {
    static constexpr bool enable_logging = ENABLE_LOGGING;
    static constexpr bool enable_verbose_code_gen = ENABLE_VERBOSE_CODE_GEN;
};

#undef ENABLE_LOGGING
#undef ENABLE_VERBOSE_CODE_GEN

} // namespace util

