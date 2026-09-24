// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_ALGORITHM_HPP_
#define LIBSBX_UTILITY_ALGORITHM_HPP_

#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <concepts>

namespace sbx::utility {

/**
 * @brief Function-object wrapper around std::sort, so a sort algorithm can be passed
 * around and swapped out as a template argument.
 */
struct std_sort {

  /**
   * @brief Sorts [first, last) with compare, forwarding any extra arguments (e.g. an
   * execution policy) through to std::sort.
   *
   * @tparam Iterator The iterator type of the range to sort.
   * @tparam Sentinel The sentinel type marking the end of the range.
   * @tparam Compare The comparison function object type.
   * @tparam Args The types of any extra arguments forwarded to std::sort.
   *
   * @param first The beginning of the range to sort.
   * @param last The end of the range to sort.
   * @param compare The comparison function object.
   * @param args Extra arguments forwarded to std::sort ahead of the range and comparator.
   */
  template<std::random_access_iterator Iterator, std::sentinel_for<Iterator> Sentinel, typename Compare = std::less<>, typename... Args>
  requires (std::sortable<Iterator, Compare>)
  auto operator()(Iterator first, Sentinel last, Compare compare = Compare{}, Args&&... args) const -> void {
    std::sort(std::forward<Args>(args)..., std::move(first), std::move(last), std::move(compare));
  }

}; // struct std_sort

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ALGORITHM_HPP_
