#pragma once

#include <utility>

namespace util {

template <typename Key, typename Value>
class static_unordered_map {
public:
    using value_type = std::pair<Key, Value>;
private:
};

} // namespace util

