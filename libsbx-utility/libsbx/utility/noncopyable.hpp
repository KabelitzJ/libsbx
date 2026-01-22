// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_NONCOPYABLE_HPP_
#define LIBSBX_UTILITY_NONCOPYABLE_HPP_

namespace sbx::utility {

struct noncopyable {

  noncopyable(const noncopyable&) = delete;

  noncopyable(noncopyable&&) noexcept = default;

  auto operator=(const noncopyable&) -> noncopyable& = delete;

  auto operator=(noncopyable&&) noexcept -> noncopyable& = default;

protected:

  noncopyable() = default;

  ~noncopyable() = default;

}; // struct noncopyable

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_NONCOPYABLE_HPP_
