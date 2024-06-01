#pragma once

#include <tuple>
#include <string>
#include <cctype>
#include <variant>
#include <algorithm>
#include <type_traits>

// Use C++20 if possible
#if __cplusplus >= 202002L
#include <bit>
#endif

namespace util {

namespace {

template <typename Original, typename... Types>
struct equivalent_integer_helper;

template <typename Original, typename Current, typename... Rest>
struct equivalent_integer_helper<Original, Current, Rest...> {
private:
    using next = equivalent_integer_helper<Original, Rest...>;
public:
	using type = std::conditional_t<sizeof(Original) == sizeof(Current), Current, typename next::type>;

	static_assert(!std::is_same_v<type, void>,
    "No equivalent integer type exists for supplied type");
};

template <typename Original, typename T>
struct equivalent_integer_helper<Original, T> {
    using type = std::conditional_t<(sizeof(T) == sizeof(Original)), T, void>;

    static_assert(!std::is_same_v<type, void>,
    "No equivalent integer type exists for supplied type");
};

// TODO: check this for performance
template <class... Args>
struct variant_cast_proxy {
    std::variant<Args...> v;

    template <class... SuperSetArgs>
    operator std::variant<SuperSetArgs...>() const {
        return std::visit([](auto&& arg) -> std::variant<SuperSetArgs...> { return arg ; },
                          v);
    }
};

} // empty namespace

// Custom type trait to check if a type is a std::tuple
template<typename T>
struct is_tuple : std::false_type {};

template<typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

// Helper to check if all arguments are tuples
template<typename... Args>
constexpr bool all_are_tuples() {
    return (is_tuple<std::decay_t<Args>>::value && ...);
}

// Helper to check if none of the arguments are tuples
template<typename... Args>
constexpr bool none_are_tuples() {
    return (!is_tuple<std::decay_t<Args>>::value && ...);
}


inline bool case_ignore_char_equal(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

inline bool case_ignore_string_equal(const std::string &a, const std::string &b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), case_ignore_char_equal);
}

// Variant converter
//
// Useful for casting between variants containing different types
template <class... Args>
auto variant_cast(const std::variant<Args...>& v) -> variant_cast_proxy<Args...> {
    return {v};
}

template <class InputIt, class T = typename std::iterator_traits<InputIt>::value_type>
bool contains(InputIt first, InputIt last, const T &value) {
    return std::find(first, last, value) != last;
}

template <class Container, class T>
bool contains(const Container &container, const T &value) {
    return contains(std::cbegin(container), std::cend(container), value);
}

template <typename To, typename From>
constexpr To bitcast(const From &from) noexcept {
// Use std::bit_cast for C++20
#if __cplusplus >= 202002L
    if constexpr (sizeof(To) == sizeof(From)) {
        return std::bit_cast<To>(from);
    } else {
        // Source: https://github.com/Cons-Cat/libCat/blob/686da771a1d2cfbdc144ded75e824c538248fbf1/src/libraries/utility/implementations/bit_cast.tpp#L30C7-L33C22
        To* p_to = static_cast<To*>(static_cast<void*>(const_cast<std::remove_const_t<From>*>(__builtin_addressof(from))));
        __builtin_memcpy(p_to, __builtin_addressof(from), sizeof(To));
        return *p_to;
    }
#else
    static_assert(sizeof(To) >= sizeof(From),
                  "Cannot bitcast to smaller size");
// Check if builtin exists; otherwise fail
#if !__has_builtin(__builtin_bit_cast)
#error "util::bitcast() implentation requires builtin-bitcast support"
#endif
#if !__has_builtin(__builtin_memcpy)
#error "util::bitcast() implentation requires builtin-memcpy support"
#endif
    if constexpr (sizeof(To) == sizeof(From)) {
        return __builtin_bit_cast(To, from);
    } else {
        // Source: https://github.com/Cons-Cat/libCat/blob/686da771a1d2cfbdc144ded75e824c538248fbf1/src/libraries/utility/implementations/bit_cast.tpp#L30C7-L33C22
        // TODO: the builtin_memcpy here needs to know that sizeof(To) == sizeof(From)
        To* p_to = static_cast<To*>(static_cast<void*>(const_cast<std::remove_const_t<From>*>(__builtin_addressof(from))));
        __builtin_memcpy(p_to, __builtin_addressof(from), sizeof(To));
        return *p_to;
    }
#endif
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
struct equivalent_integer {
    using type = typename equivalent_integer_helper<T, int, long, long long>::type;
};

template <typename T>
using equivalent_integer_t = typename equivalent_integer<T>::type;

} // namespace util

