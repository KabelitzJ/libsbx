// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_NONCOPYABLE_HPP_
#define LIBSBX_UTILITY_NONCOPYABLE_HPP_

namespace sbx::utility {

class noncopyable {

protected:

  noncopyable() = default;

  ~noncopyable() = default;

  noncopyable(const noncopyable&) = delete;

  noncopyable(noncopyable&&) = default;

  auto operator=(const noncopyable&) -> noncopyable& = delete;

  auto operator=(noncopyable&&) -> noncopyable& = default;

}; // class noncopyable

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_NONCOPYABLE_HPP_
