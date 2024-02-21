#include <arancini/util/system-config.h>

#include <mutex>
#include <iostream>
#include <functional>
#include <type_traits>

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
    enum class levels : uint8_t {
        disabled,
        debug,
        info,
        warn,
        error,
        fatal
    };

    levels set_level(levels level) {
        level_ = level;
        return level_;
    }

    template<typename... Args>
    T &debug(Args&&... args) {
        if (level_ <= levels::debug)
            return static_cast<T*>(this)->log("[DEBUG] ", std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &info(Args&&... args) {
        if (level_ <= levels::info)
            return static_cast<T*>(this)->log("[INFO] ", std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &warn(Args&&... args) {
        if (level_ <= levels::warn)
            return static_cast<T*>(this)->log("[WARNING] ", std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &error(Args&&... args) {
        if (level_ <= levels::error)
            return static_cast<T*>(this)->log("[ERROR] ", std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &fatal(Args&&... args) {
        if (level_ <= levels::fatal)
            return static_cast<T*>(this)->log("[FATAL] ", std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }
protected:
    levels level_ = levels::warn;

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
        enabled_ = status;
        return enabled_;
    }

    bool is_enabled() const { return enabled_; }

    // Basic interface for logging
    //
    // Prints specified arguments with cout
    template<typename... Args>
    base_type &log(Args&&... args) {
        lock_policy::lock();
        if (enabled_) {
            std::cout << prefix_ << ' ';
            ((std::cout << (eval(args)) << ' '), ...);
            std::cout << '\n';
        }
        lock_policy::unlock();

        return *this;
    }
private:
    bool enabled_ = true;
    std::string prefix_;

    // Helper functions for uniformly handling functions and non-functions
    template<typename T>
    static auto eval(const T& arg) -> std::enable_if_t<!std::is_invocable_v<T>, const T&> {
        return arg;
    }

    // Helper for function objects
    template<typename T>
    static auto eval(const T& arg) -> std::enable_if_t<std::is_invocable_v<T>, decltype(arg())> {
        return arg();
    }
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
    using base_type = logger_impl<true, no_lock_policy, level_policy>;

    bool enable(bool) { return false; }
    bool is_enabled() const { return false; }

    // FIXME: add attribute to specify that it should be ignored
    template<typename... Args>
    logger_impl<false, no_lock_policy, level_policy> &log([[gnu::unused]] Args&&... args) { return *this; }
private:
    template<typename T>
    static auto eval(const T& arg) -> std::enable_if_t<!std::is_invocable_v<T>, const T&> {
        return arg;
    }

    // Helper for function objects
    template<typename T>
    static auto eval(const T& arg) -> std::enable_if_t<std::is_invocable_v<T>, decltype(arg())> {
        return arg();
    }
};

// Lazy evaluation handlers
//
// Helper for non-const lazy evaluation
template <typename... Args>
struct non_const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...), T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

// Lazy evaluation handlers
//
// Helper for const lazy evaluation
template <typename... Args>
struct const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...) const, const T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

// Lazy evaluation handlers
//
// Uniformly handle const and non-const lazy evaluation
//
// Supports lazy evaluation for both regular functions and class member functions
template <typename... Args>
struct lazy_eval_impl : const_lazy_eval_impl<Args...>, non_const_lazy_eval_impl<Args...>
{
    using const_lazy_eval_impl<Args...>::operator();
    using non_const_lazy_eval_impl<Args...>::operator();
    template <typename R>
    auto operator()(R (*ptr)(Args...), Args&&... args) const noexcept
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

// Basic logger type with associated policy
using basic_logging = details::logger_impl<system_config::enable_logging, details::no_lock_policy, details::level_policy>;

// Global logger for generic logging in the project
inline basic_logging global_logger;

// Perfect forward initialization sometimes doesn't work (e.g. packed structs)
// In such cases, an explicit copy is needed
template <typename T>
T copy(const T& t) { return t; }

} // namespace util

