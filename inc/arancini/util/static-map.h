#pragma once

#include <fmt/core.h>

#include <array>
#include <utility>

namespace util {

template <typename Key, std::size_t Size>
struct perfect_hash {
    [[nodiscard]]
    constexpr std::size_t operator()(const Key& k) const {
        return 1 % Size;
    }
};

template <typename Key, typename Value, std::size_t Size,
          typename Hash = perfect_hash<Key, Size>>
class static_unordered_map final {
    using map_array = typename std::array<std::pair<Key, Value>, Size>;
public:
    using value_type = std::pair<Key, Value>;

    template <typename... Pairs>
    constexpr static_unordered_map(Hash hasher = Hash(), Pairs... pairs):
        data_({pairs...}),
        hasher_(hasher)
    {
        static_assert(sizeof...(Pairs) == Size, "number of pairs must match size");
    }

    template <typename... Pairs>
    constexpr static_unordered_map(Pairs... pairs):
        static_unordered_map(Hash(), std::forward<Pairs>(pairs)...)
    { }

    using iterator = typename map_array::iterator;
    using const_iterator = typename map_array::const_iterator;

    [[nodiscard]] iterator begin() { return data_.begin(); }
    [[nodiscard]] const_iterator begin() const { return data_.begin(); }
    [[nodiscard]] const_iterator cbegin() const { return data_.cbegin(); }

    [[nodiscard]] iterator end() { return data_.end(); }
    [[nodiscard]] const_iterator end() const { return data_.end(); }
    [[nodiscard]] const_iterator cend() const { return data_.cend(); }

    // TODO: fix
    [[nodiscard]] iterator rbegin() { return data_.rbegin(); }
    [[nodiscard]] const_iterator rbegin() const { return data_.rbegin(); }
    [[nodiscard]] const_iterator crbegin() const { return data_.crbegin(); }

    [[nodiscard]] iterator rend() { return data_.rend(); }
    [[nodiscard]] const_iterator rend() const { return data_.rend(); }
    [[nodiscard]] const_iterator crend() const { return data_.crend(); }

    // TODO: provide C++20-like versions
    iterator find(const Key& k) {
        for (auto it = begin(); it != end(); ++it) {
            if (k == it->first) return it;
        }

        return end();
    }

    const_iterator find(const Key& k) const {
        for (auto it = begin(); it != end(); ++it) {
            if (k == it->first) return it;
        }

        return end();
    }

    [[nodiscard]]
    Value& at(const Key& k) {
        auto it = find(k);

        [[likely]]
        if (it != end()) return it->second;

        throw std::out_of_range(fmt::format("Container does not contain key {}", k));
    }

    [[nodiscard]]
    const Value& at(const Key& k) const {
        auto it = find(k);

        [[likely]]
        if (it != end()) return it->second;

        throw std::out_of_range(fmt::format("Container does not contain key {}", k));
    }

    [[nodiscard]]
    bool contains(const Key& k) const { return find(k) != end(); }

    [[nodiscard]]
    std::size_t count(const Key& k) const { return contains(k); }

    [[nodiscard]]
    constexpr std::size_t size() const { return Size; }

    [[nodiscard]]
    constexpr std::size_t max_size() const { return size(); }

    [[nodiscard]]
    constexpr bool empty() const { return size() == 0; }

    [[nodiscard]]
    Hash hash_function() const { return hasher_; }

    [[nodiscard]]
    constexpr float load_factor() const { return 1; }

    [[nodiscard]]
    const float max_load_factor() const { return 1; }
private:
    std::array<std::pair<Key, Value>, Size> data_;
    Hash hasher_;
};

// Deduction Guide (C++17+)
template <typename... Pairs, typename Hash>
static_unordered_map(Pairs..., Hash) -> static_unordered_map<
    typename std::tuple_element<0, std::tuple<Pairs...>>::type::first_type,
    typename std::tuple_element<0, std::tuple<Pairs...>>::type::second_type,
    sizeof...(Pairs),
    Hash>;

template <typename... Pairs>
static_unordered_map(Pairs...)
 -> static_unordered_map<
      typename std::decay_t<typename std::tuple_element<0, std::tuple<Pairs...>>::type>::first_type,
      typename std::decay_t<typename std::tuple_element<0, std::tuple<Pairs...>>::type>::second_type,
      sizeof...(Pairs),
      /* default Hash type here, e.g.: */ perfect_hash<
          typename std::decay_t<typename std::tuple_element<0, std::tuple<Pairs...>>::type>::first_type,
          sizeof...(Pairs)
      >
   >;

} // namespace util

