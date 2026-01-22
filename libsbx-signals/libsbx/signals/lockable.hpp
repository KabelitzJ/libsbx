// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_LOCKABLE_HPP_
#define LIBSBX_SIGNAL_LOCKABLE_HPP_

#include <mutex>

namespace sbx::signals {

template<typename Type>
concept lockable = requires(Type& value) {
  { value.try_lock() } -> std::same_as<bool>;
  { value.lock() } -> std::same_as<void>;
  { value.unlock() } -> std::same_as<void>;
}; // concept lockable

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

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_LOCKABLE_HPP_
