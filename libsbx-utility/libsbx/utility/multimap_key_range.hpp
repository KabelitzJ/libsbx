// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_MULTIMAP_KEY_RANGE_HPP_
#define LIBSBX_UTILITY_MULTIMAP_KEY_RANGE_HPP_

#include <utility>
#include <tuple>
#include <concepts>
#include <type_traits>

namespace sbx::utility {

template<typename Map>
concept multimap = requires {
    typename Map::key_type;
    typename Map::iterator;
    typename Map::const_iterator;
} && requires(Map& map, const Map::key_type& key) {
    { map.equal_range(key) };
};

template<multimap Map>
struct multimap_key_range {

public:

    using key_type = typename Map::key_type;
    using iterator = std::conditional_t<std::is_const_v<Map>, typename Map::const_iterator, typename Map::iterator>;

    multimap_key_range(Map& range, const key_type& key) {
        std::tie(_begin, _end) = range.equal_range(key);
    }

    auto begin() -> iterator {
        return _begin;
    }

    auto end() -> iterator {
        return _end;
    }

private:

    iterator _begin;
    iterator _end;

};

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_MULTIMAP_KEY_RANGE_HPP_
