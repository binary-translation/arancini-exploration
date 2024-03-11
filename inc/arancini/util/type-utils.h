#pragma once

#include <tuple>
#include <string>
#include <cctype>
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

} // namespace util
  
