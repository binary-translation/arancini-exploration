#pragma once

#include <arancini/util/type-utils.h>
#include <arancini/util/system-config.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <mutex>
#include <tuple>
#include <bitset>
#include <functional>
#include <filesystem>

namespace util {

namespace details {

// Dummy locking policy for logger
//
// This policy is used on non-synchronizing loggers for a lock()/unlock()
// interface
class no_lock_policy {
public:
    void lock() { }
    void unlock() { }
};

// Locking policy for logger
//
// This policy handles synchronization for the logger using a mutex via the
// lock()/unlock() interface that is invoked by the logger implementation
class basic_lock_policy {
public:
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
private:
    std::mutex mutex_;
};

// Logging levels policy for logger
//
// This policy provides an alternative interface to the logger with a more
// user-friendly API.
template <typename T>
class level_policy {
public:
    struct levels_t {
        enum level : uint8_t {
        disabled,
        debug,
        info,
        warn,
        error,
        fatal
        };

        level lowest_level = level::fatal;
        level highest_level = level::debug;
    };

    using levels = typename levels_t::level;

    level_policy() = default;
    level_policy(levels level): level_(level) { }

    levels set_level(levels level) {
        level_ = level;
        return level_;
    }

    levels get_level() const { return level_; }

    template<typename... Args>
    T &debug(Args&&... args) {
        if (level_ <= levels::debug)
            return logger->log(stderr,
                               std::forward_as_tuple("[DEBUG]   "),
                               std::forward_as_tuple(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &info(Args&&... args) {
        if (level_ <= levels::info)
            return logger->log(stderr,
                               std::forward_as_tuple("[INFO]    "),
                               std::forward_as_tuple(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &warn(Args&&... args) {
        if (level_ <= levels::warn)
            return logger->log(stderr,
                               std::forward_as_tuple("[WARNING] "),
                               std::forward_as_tuple(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &error(Args&&... args) {
        if (level_ <= levels::error)
            return logger->log(stderr,
                               std::forward_as_tuple("[ERROR]   "),
                               std::forward_as_tuple(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &fatal(Args&&... args) {
        // Print FATAL messages even with disabled logger
        if (level_ <= levels::fatal) {
            return logger->forced_log(stderr,
                                      std::forward_as_tuple("[FATAL]   "),
                                      std::forward_as_tuple(std::forward<Args>(args)...));
        }
        return *logger;
    }
protected:
    levels level_ = levels::warn;

    T* logger = static_cast<T*>(this);

    virtual ~level_policy() { }
};


// Logger implementation using policies
//
// enabled: compile-time enable/disable setting for the logger
// A disabled logger can still print messages using the forced_log() and
// forced_unsynch_log() methods
//
// lock_policy: must provide lock() and unlock() - can be used for
// synchronization
//
// level_policy: provides different interface to logger (e.g. info() instead of
// log())
//
// Logger can be customized with different policies
template <bool enabled, typename lock_policy, template <typename logger> typename level_policy>
class logger_impl:
private lock_policy,
public level_policy<logger_impl<enabled, lock_policy, level_policy>>
{
public:
    using base_type = logger_impl<enabled, lock_policy, level_policy>;
    using level_policy_type = level_policy<logger_impl<enabled, lock_policy, level_policy>>;

    logger_impl() = default;

    // Specify a prefix for all printed messages
    logger_impl(const std::string &prefix, bool enable = false,
                typename level_policy_type::levels level = level_policy_type::levels::lowest_level):
        prefix_(prefix),
        enabled_(enable),
        level_policy_type(level)
    {
    }

    // Enable/disable logger
    //
    // Does nothing when logger is disabled at compile-time
    bool enable(bool status) {
        if constexpr (enabled) {
            std::lock_guard<lock_policy> lock(*this);
            return (enabled_ = status);
		} else {
			return false;
		}
	}

    bool is_enabled() const { return enabled_; }

    // Basic interface for logging
    //
    // Enables logging even when the logger is disabled
    // Meant for use only be wrappers or in special other cases
    template<typename... Args>
    base_type &forced_log(FILE* dest, Args&&... args) {
        std::lock_guard<lock_policy> lock(*this);
        return forced_unsynch_log(dest, std::forward<Args>(args)...);
    }

    // Basic interface for logging
    //
    // Enables logging even when the logger is disabled
    // No synchronization is used; irrespective of the lock policy
    //
    // Meant for use only be wrappers or in special other cases
    template<typename... Args>
    base_type &forced_unsynch_log(FILE* dest, Args&&... args) {
        static_assert((all_are_tuples<Args...>() || none_are_tuples<Args...>()),
                  "Arguments must be all tuples or no tuples, not a mix.");

        if (!prefix_.empty()) fmt::print(dest, "{}", prefix_);

        if constexpr (all_are_tuples<Args...>()) {
            ((print(dest, std::forward<Args>(args))), ...);
        } else if constexpr (none_are_tuples<Args...>()) {
            print(dest, std::forward_as_tuple(args...));
        }

        return *this;

    }

    // Basic canonical interface for logging
    //
    // Support explicitly specifying the destination FILE* for the output
    // Arguments are given as for the {fmt} library
    template<typename... Args>
    base_type &log(FILE* dest, Args&&... args) {
        if constexpr (enabled) {
            if (enabled_) {
                return forced_log(dest, std::forward<Args>(args)...);
            }
        }

        return *this;
    }

    // Basic canonical interface for logging for printing to stdout
    //
    // Arguments are given as for the {fmt} library
    template<typename... Args>
    base_type &log(Args&&... args) {
        return log(stdout, std::forward<Args>(args)...);
    }
private:
    bool enabled_ = enabled;
    std::string prefix_;

    template <typename... Args>
    void print(FILE* dest, std::tuple<Args...>&& printer_args) {
        std::apply([dest](auto&& format, auto&&... args) {
               fmt::print(dest, std::forward<decltype(format)>(format), std::forward<decltype(args)>(args)...);
           }, printer_args);
    }


    template <typename... Args>
    void print(FILE* dest, Args&&... printer_args) {
        print(dest, std::forward_as_tuple(printer_args...));
    }
};

// Lazy evaluation handlers
//
// Helper for non-const lazy evaluation
template <typename... Args>
struct non_const_lazy_eval_impl {
    template <typename R, typename T>
    std::function<R()> operator()(R (T::*ptr)(Args...), T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

// Lazy evaluation handlers
//
// Helper for const lazy evaluation
template <typename... Args>
struct const_lazy_eval_impl {
    template <typename R, typename T>
    std::function<R()> operator()(R (T::*ptr)(Args...) const, const T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

// Lazy evaluation handlers
//
// Uniformly handle const and non-const lazy evaluation
//
// Supports lazy evaluation for both regular functions and class member functions
template <typename... Args>
struct lazy_eval_impl : const_lazy_eval_impl<Args...>, non_const_lazy_eval_impl<Args...> {
    using const_lazy_eval_impl<Args...>::operator();
    using non_const_lazy_eval_impl<Args...>::operator();
    template <typename R>
    std::function<R()> operator()(R (*ptr)(Args...), Args&&... args) const noexcept
    { return std::bind(ptr, args...); }
};

} // namespace details

// Lazy evaluation wrappers for postponing evaluation until use by the logger
// By default, all loggers evaluate their arguments
//
// These wrappers can express that a given parameter should not be evaluated
// immediately
template <typename... Args> constexpr details::lazy_eval_impl<Args...> lazy_eval = {};
template <typename... Args> constexpr details::const_lazy_eval_impl<Args...> const_lazy_eval = {};
template <typename... Args> constexpr details::non_const_lazy_eval_impl<Args...> non_const_lazy_eval = {};

// Basic logger types with associated policies
using basic_logging = details::logger_impl<system_config::enable_global_logging, details::no_lock_policy, details::level_policy>;
using synch_logging = details::logger_impl<system_config::enable_global_logging, details::basic_lock_policy, details::level_policy>;

// Global logger for generic logging in the project
using global_logging = synch_logging;
inline global_logging global_logger;

// Perfect forward initialization sometimes doesn't work (e.g. packed structs)
// In such cases, an explicit copy is needed
template <typename T>
T copy(const T& t) { return t; }

inline std::string logging_separator(char separator_character = '=', std::size_t repeats = 80) {
    return std::string(repeats, separator_character);
}

} // namespace util

template <typename R>
struct fmt::formatter<std::function<R()>> : fmt::formatter<R> {
    format_context::iterator  format(const std::function<R()> &binding, format_context &format_ctx) const {
        return format_to(format_ctx.out(), "{}", binding());
    }
};

// Extensions to fmt

// Handle printing filesystem paths
// Printer introduced in FMT_VERSION 10.2.1
#if FMT_VERSION <= 100202
template <>
struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const std::filesystem::path& path, FormatContext& ctx) const {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};
#endif

// Handle printing std::bitset
// {fmt} supports std::bitset only on its main branch (latest release: 10.2.1)
#if FMT_VERSION < 100202
template <std::size_t N, typename Char>
struct fmt::formatter<std::bitset<N>, Char> : fmt::formatter<std::string_view> {
public:
    template <typename FormatContext>
    auto format(const std::bitset<N>& bs, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", bs.to_string());
    }
};
#endif

