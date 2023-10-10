#include <memory>
#include <iostream>
#include <functional>
#include <type_traits>

namespace utils {

template <bool enable>
class logger_impl;

template <>
class logger_impl<true> {
public:
    bool enable(bool status) { enabled_ = status; return enabled_; }

    template<typename... Args>
    void log(Args&&... args) {
        if (enabled_) {
            ((std::cout << ' ' << (eval(args))), ...);
            std::cout << '\n';
        }
    }

    template<typename... Args>
    void debug(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal(Args&&... args) {
        log(std::forward<Args>(args)...);
    }
private:
    bool enabled_ = true;

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

template <>
class logger_impl<false> {
public:
    bool enable(bool) { return false; }

    template<typename... Args>
    void log(Args&&... args) { }

    template<typename... Args>
    void debug(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal(Args&&... args) {
        log(std::forward<Args>(args)...);
    }
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

template <typename... Args>
struct non_const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...)) const noexcept -> decltype(ptr)
    { return ptr; }
    template <typename R>
    static constexpr auto of(R (*ptr)(Args...)) noexcept -> decltype(ptr)
    { return ptr; }
};

template <typename... Args>
struct const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...) const) const noexcept -> decltype(ptr)
    { return ptr; }
    template <typename R>
    static constexpr auto of(R (*ptr)(Args...)) noexcept -> decltype(ptr)
    { return ptr; }
};

template <typename... Args>
struct lazy_eval_impl : const_lazy_eval_impl<Args...>, non_const_lazy_eval_impl<Args...>
{
    using const_lazy_eval_impl<Args...>::operator();
    using non_const_lazy_eval_impl<Args...>::operator();
    template <typename R>
    auto operator()(R (*ptr)(Args...)) const noexcept -> decltype(ptr)
    { return ptr; }
    template <typename R>
    static constexpr auto of(R (*ptr)(Args...)) noexcept -> decltype(ptr)
    { return ptr; }
};

template <typename... Args> constexpr lazy_eval_impl<Args...> lazy_eval = {};
template <typename... Args> constexpr const_lazy_eval_impl<Args...> const_lazy_eval = {};
template <typename... Args> constexpr non_const_lazy_eval_impl<Args...> non_const_lazy_eval = {};

#ifndef ENABLE_LOG
#define ENABLE_LOG false
#else
#define ENABLE_LOG true
#endif // ENABLE_LOG

inline logger_impl<ENABLE_LOG> logger;

} // namespace utils

