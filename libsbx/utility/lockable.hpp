// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_LOCKABLE_HPP_
#define LIBSBX_UTILITY_LOCKABLE_HPP_

#include <mutex>

namespace sbx::utility {

/**
 * @brief A type usable with std::lock_guard/std::unique_lock: try_lock, lock and unlock.
 *
 * @tparam Type The type to check.
 */
template<typename Type>
concept lockable = requires(Type& value) {
  { value.try_lock() } -> std::same_as<bool>;
  { value.lock() } -> std::same_as<void>;
  { value.unlock() } -> std::same_as<void>;
}; // concept lockable

/**
 * @brief A no-op mutex satisfying lockable, for templates that need a lock type but are
 * used in a single-threaded context.
 */
struct null_mutex {
  null_mutex() noexcept = default;
  null_mutex(const null_mutex& other) = delete;
  null_mutex(null_mutex&& other) noexcept = delete;

  ~null_mutex() noexcept = default;

  auto operator=(const null_mutex& other) -> null_mutex& = delete;
  auto operator=(null_mutex&& other) noexcept -> null_mutex& = delete;

  inline auto try_lock() noexcept -> bool { return true; }
  inline auto lock() noexcept -> void {}
  inline auto unlock() noexcept -> void {}
}; // struct null_mutex

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_LOCKABLE_HPP_
