// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ALGORITHM_HPP_
#define LIBSBX_UTILITY_ALGORITHM_HPP_

#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <concepts>

namespace sbx::utility {

struct std_sort {

  template<std::random_access_iterator Iterator, std::sentinel_for<Iterator> Sentinel, typename Compare = std::less<>, typename... Args>
  auto operator()(Iterator first, Sentinel last, Compare compare = Compare{}, Args&&... args) const -> void {
    std::sort(std::forward<Args>(args)..., std::move(first), std::move(last), std::move(compare));
  }

}; // struct std_sort

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ALGORITHM_HPP_
