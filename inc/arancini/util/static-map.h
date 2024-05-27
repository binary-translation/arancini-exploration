#pragma once

#include <array>
#include <stdexcept>
#include <algorithm>

namespace util {

// This needs to be refactored
//
// TODO:
// 1. Use std::hash (or alternative) for O(1) lookups - use perfect hashing
// 2. Implement deduction guide (to get the same initialization behaviour as with std::array)
template <typename Key, typename Value, std::size_t Size>
class static_map final {
public:
    using value_type = typename std::pair<Key, Value>;
    using base_type = typename std::array<value_type, Size>;

    using size_type = typename base_type::size_type;
    using iterator = typename base_type::iterator;
    using const_iterator = typename base_type::const_iterator;

    template <typename... Pairs>
    constexpr static_map(Pairs&&... pairs): array_{{std::forward<Pairs>(pairs)...}} {
        static_assert(sizeof...(Pairs) <= Size, "Incorrect number of initialization pairs.");
    }

    const_iterator find(const Key &k) const {
        return std::find_if(array_.cbegin(), array_.cend(), [&k](const value_type &v) {
                                return v.first == k;
                            });
    }

    iterator find(const Key &k) {
        return std::find_if(array_.begin(), array_.end(), [&k](const value_type &v) {
                                return v.first == k;
                            });
    }

    Value &get(const Key &k, Value &fallback) {
        auto it = find(k);
        if (it != end()) return it->second;
        return fallback;
    }

    const Value &get(const Key &k, const Value &fallback) const {
        auto it = find(k);
        if (it != end()) return it->second;
        return fallback;
    }

    Value &at(const Key &k) {
        auto it = find(k);
        if (it != end()) return it->second;
        throw std::out_of_range("static_map detected out of bounds access");
    }

    bool contains(const Key &k) const { return find(k) != end(); }

    const Value &at(const Key &k) const {
        auto it = find(k);
        if (it != end()) return it->second;
        throw std::out_of_range("static_map detected out of bounds access");
    }

    iterator begin() { return array_.begin(); }
    iterator end() { return array_.end(); }

    const_iterator begin() const { return array_.cbegin(); }
    const_iterator end() const { return array_.cend(); }

    // TA: FIX
    size_type size() const { return Size; }
    size_type max_size() const { return Size; }

private:
    std::array<value_type, Size> array_;
};

} // namespace util

