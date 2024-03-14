#pragma once

#include <array>
#include <algorithm>

namespace util {

template <typename Key, typename Value, std::size_t Size>
class static_map final {
public:
    using value_type = typename std::pair<Key, Value>;
    using base_type = typename std::array<value_type, Size>;

    using size_type = typename base_type::size_type;
    using iterator = typename base_type::const_iterator;
    using const_iterator = iterator;

    static_map(std::initializer_list<value_type> values) {
        // TODO: optimize
        std::copy(values.begin(), values.end(), array_.begin());
    }
    
    const_iterator find(const Key &k) const { 
        return std::find_if(array_.begin(), array_.end(), [&k](const value_type &v) {
                                return v.first == k;
                            });
    }

    const Value &get(const Key &k, const Value &fallback) {
        auto it = find(k);
        if (it != end()) return it->second;
        return fallback;
    }

    iterator begin() const { return array_.begin(); }
    iterator end() const { return array_.end(); }

    // TA: FIX
    bool size() const { return Size; }
    bool max_size() const { return Size; }

private:
    std::array<value_type, Size> array_;
};

} // namespace util

