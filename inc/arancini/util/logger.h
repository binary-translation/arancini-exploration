#pragma once

#include <arancini/util/system-config.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <mutex>
#include <functional>

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
    };

    using levels = typename levels_t::level;

    levels set_level(levels level) {
        level_ = level;
        return level_;
    }

    levels get_level() const { return level_; }

    T &separator(levels level, char separator_character) {
        if (level_ <= level) {
            std::string separator(80, separator_character);
            return static_cast<T*>(this)->log("{}\n", separator);
        }
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &debug(Args&&... args) {
        if (level_ <= levels::debug)
            return logger->log(stderr, "[DEBUG]   {}", fmt::format(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &info(Args&&... args) {
        if (level_ <= levels::info)
            return logger->log(stderr, "[INFO]    {}", fmt::format(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &warn(Args&&... args) {
        if (level_ <= levels::warn)
            return logger->log(stderr, "[WARNING] {}", fmt::format(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &error(Args&&... args) {
        if (level_ <= levels::error)
            return logger->log(stderr, "[ERROR]   {}", fmt::format(std::forward<Args>(args)...));
        return *logger;
    }

    template<typename... Args>
    T &fatal(Args&&... args) {
        // Print FATAL messages even with disabled logger
        if (!logger->is_enabled()) {
            return logger->mandatory_log(stderr, "[FATAL]   {}", fmt::format(std::forward<Args>(args)...));
        }

        if (level_ <= levels::fatal)
            return logger->log(stderr, "[FATAL]   {}", fmt::format(std::forward<Args>(args)...));
        return *logger;
    }
protected:
    levels level_ = levels::warn;

    T* logger = static_cast<T*>(this);

    virtual ~level_policy() { }
};

template <bool enable, typename lock_policy, template <typename logger> typename level_policy>
class logger_impl;

// Logger implementation using policies
//
// lock_policy: must provide lock() and unlock() - can be used for
// synchronization
//
// level_policy: provides different interface to logger (e.g. info() instead of
// log())
//
// Logger can be customized with different policies
template <typename lock_policy, template <typename logger> typename level_policy>
class logger_impl<true, lock_policy, level_policy> final :
private lock_policy,
public level_policy<logger_impl<true, lock_policy, level_policy>>
{
public:
    logger_impl() = default;

    // Specify a prefix for all printed messages
    logger_impl(const std::string &prefix): prefix_(prefix) { }

    using base_type = logger_impl<true, lock_policy, level_policy>;

    // Enable/disable logger
    bool enable(bool status) {
        lock_policy::lock();
        enabled_ = status;
        return enabled_;
        lock_policy::unlock();
    }

    bool is_enabled() const { return enabled_; }

    // Basic interface for logging
    //
    // Enables logging even when the logger is disabled
    // Meant for use only be wrappers or in special other cases
    template<typename... Args>
    base_type &mandatory_log(FILE* dest, Args&&... args) {
        lock_policy::lock();
        if (!prefix_.empty()) fmt::print("{}", prefix_);
        fmt::print(dest, std::forward<Args>(args)...);
        lock_policy::unlock();

        return *this;
    }

    // Basic canonical interface for logging
    //
    // Support explicitly specifying the destination FILE* for the output
    // Arguments are given as for the {fmt} library
    template<typename... Args>
    base_type &log(FILE* dest, Args&&... args) {
        if (enabled_) {
            return mandatory_log(dest, std::forward<Args>(args)...);
        }

        return *this;
    }
    
    // Basic interface for logging
    template<typename... Args>
    base_type &log(Args&&... args) {
        return log(stdout, std::forward<Args>(args)...);
    }
private:
    bool enabled_ = true;
    std::string prefix_;
};

// Dummy logger
//
// Does not print any messages, meant for compile-time disabling of the logger
//
// Maintains the same interface as the actual logger
template <template <typename T> typename level_policy>
class logger_impl<false, no_lock_policy, level_policy> final :
public no_lock_policy,
public level_policy<logger_impl<false, no_lock_policy, level_policy>>
{
public:
    using base_type = logger_impl<false, no_lock_policy, level_policy>;
    
    // Specify a prefix for all printed messages
    logger_impl() = default;
    logger_impl(const std::string &prefix): prefix_(prefix) { }

    bool enable(bool) { return false; }
    bool is_enabled() const { return false; }

    // FIXME: add attribute to specify that it should be ignored
    template<typename... Args>
    base_type &log([[gnu::unused]] Args&&...) { return *this; }

    template<typename... Args>
    base_type &log(FILE*, [[gnu::unused]] Args&&...) { return *this; }

    // Basic interface for logging
    template<typename... Args>
    base_type &mandatory_log(FILE* dest, Args&&... args) {
        if (!prefix_.empty()) fmt::print("{}", prefix_);
        fmt::print(dest, std::forward<Args>(args)...);
        return *this;
    }
private:
    std::string prefix_;
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
inline basic_logging global_logger;

// Perfect forward initialization sometimes doesn't work (e.g. packed structs)
// In such cases, an explicit copy is needed
template <typename T>
T copy(const T& t) { return t; }

} // namespace util

template <typename R>
struct fmt::formatter<std::function<R()>> : fmt::formatter<R> {
    format_context::iterator  format(const std::function<R()> &binding, format_context &format_ctx) const {
        return format_to(format_ctx.out(), "{}", binding());
    }
};

