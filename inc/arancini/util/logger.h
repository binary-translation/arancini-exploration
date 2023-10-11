#include <arancini/util/system-config.h>

#include <mutex>
#include <memory>
#include <iostream>
#include <functional>
#include <type_traits>

namespace util {

namespace details {

class no_lock_policy {
public:
    void lock() { }
    void unlock() { }
};

class basic_lock_policy {
public:
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
private:
    std::mutex mutex_;
};

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
            return static_cast<T*>(this)->log(std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &info(Args&&... args) {
        if (level_ <= levels::info)
            return static_cast<T*>(this)->log(std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &warn(Args&&... args) {
        if (level_ <= levels::warn)
            return static_cast<T*>(this)->log(std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &error(Args&&... args) {
        if (level_ <= levels::error)
            return static_cast<T*>(this)->log(std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }

    template<typename... Args>
    T &fatal(Args&&... args) {
        if (level_ <= levels::fatal)
            return static_cast<T*>(this)->log(std::forward<Args>(args)...);
        else return *static_cast<T*>(this);
    }
protected:
    levels level_ = levels::warn;

    virtual ~level_policy() { }
};

template <bool enable, typename lock_policy, template <typename logger> typename level_policy>
class logger_impl;

template <typename lock_policy, template <typename logger> typename level_policy>
class logger_impl<true, lock_policy, level_policy> final :
public lock_policy,
public level_policy<logger_impl<true, lock_policy, level_policy>>
{
public:
    using base_type = logger_impl<true, lock_policy, level_policy>;

    bool enable(bool status) {
        enabled_ = status;
        return enabled_;
    }

    bool is_enabled() const { return enabled_; }

    template<typename... Args>
    base_type &log(Args&&... args) {
        lock_policy().lock();
        if (enabled_) {
            ((std::cout << (eval(args)) << ' '), ...);
            std::cout << '\n';
        }
        lock_policy().unlock();

        return *this;
    }
private: bool enabled_ = true;

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

template <typename... Args>
struct non_const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...), T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

template <typename... Args>
struct const_lazy_eval_impl {
    template <typename R, typename T>
    auto operator()(R (T::*ptr)(Args...) const, const T* obj, Args&&... args) const noexcept
    { return std::bind(ptr, obj, args...); }
};

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

template <typename... Args> constexpr details::lazy_eval_impl<Args...> lazy_eval = {};
template <typename... Args> constexpr details::const_lazy_eval_impl<Args...> const_lazy_eval = {};
template <typename... Args> constexpr details::non_const_lazy_eval_impl<Args...> non_const_lazy_eval = {};

using logging = details::logger_impl<util::system_config::enable_logging, details::no_lock_policy, details::level_policy>;
inline logging logger;

} // namespace util

