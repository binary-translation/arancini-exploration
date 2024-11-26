#pragma once

#include <tuple>
#include <string>
#include <cctype>
#include <cstring>
#include <variant>
#include <algorithm>
#include <type_traits>

namespace util {

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

template <typename Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(e);
}

template <typename To, typename From>
To variant_cast(From&& from) {
    return std::visit(
        [](auto&& elem) -> To { return To(std::forward<decltype(elem)>(elem)); },
        std::forward<From>(from));
}

enum class endian {
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER
};

// helper type for the visitor #4
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Based on: https://github.com/jfbastien/bit_cast
template<typename To, typename From>
inline constexpr To bit_cast_zeros(const From& from) noexcept {
    To to;

    if constexpr (endian::native == endian::little) {
        std::memcpy(&to, &from, sizeof(From));
        return to;
    }

    if constexpr (endian::native == endian::big) {
        static_assert(0, "Big-endian architectures not supported by Arancini");
    }
}

} // namespace util

