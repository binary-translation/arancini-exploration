#pragma once

#include <fmt/format.h>

#ifndef ENABLE_GLOBAL_LOGGING
#define ENABLE_GLOBAL_LOGGING false
#endif // ENABLE_GLOBAL_LOGGING

namespace util {

class system_config {
public:
    static constexpr bool enable_global_logging = ENABLE_GLOBAL_LOGGING;

    bool set_logging(bool state) {
        if (enable_global_logging)
            return (is_logging_ = state);
        return false;
    }

    bool set_chaining(bool state) {
        return (is_chaining_ = state);
    }

    bool set_optimize_flag(bool state) {
        return (is_optimize_flags_ = state);
    }

    [[nodiscard]]
    bool is_logging() const { return is_logging_; }

    [[nodiscard]]
    bool is_chaining() const { return is_chaining_; }

    [[nodiscard]]
    bool is_optimizing_flags() const { return is_optimize_flags_; }

    [[nodiscard]]
    static system_config& get() {
        static system_config sysconf_;
        return sysconf_;
    }
private:
    bool is_logging_ = false;
    bool is_chaining_ = true;
    bool is_optimize_flags_ = true;

    bool handle_enable_log();

    bool handle_log_level();

    bool handle_log_stream();

    bool handle_chaining();

    bool handle_optimize_flags();

    system_config() {
        if (enable_global_logging) {
            handle_enable_log();
            handle_log_level();
            handle_log_stream();
        }

        handle_chaining();

        handle_optimize_flags();
    }
};

} // namespace util

template <>
struct fmt::formatter<util::system_config> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const util::system_config& sysconf, FormatContext& ctx) const {
        fmt::format_to(ctx.out(), "Global Log enabled: {}\n", sysconf.is_logging());
        fmt::format_to(ctx.out(), "Chaining optimization enabled: {}\n", sysconf.is_chaining());
        fmt::format_to(ctx.out(), "Flag optimization enabled: {}\n", sysconf.is_optimizing_flags());

        return ctx.out();
    }
};

