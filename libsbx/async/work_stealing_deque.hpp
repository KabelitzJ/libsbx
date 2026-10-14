// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASYNC_WORK_STEALING_DEQUE_HPP_
#define LIBSBX_ASYNC_WORK_STEALING_DEQUE_HPP_

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/cache.hpp>

namespace sbx::async {

/**
 * @brief A Chase-Lev work-stealing deque with the weak-memory orderings from Lê et al., "Correct and Efficient Work-Stealing for Weak Memory Models" (2013).
 *
 * Only the owning thread may push() and pop(), at the bottom (LIFO). Any thread may steal() from the top (FIFO).
 * The capacity is fixed, so push() fails when full and the caller falls back elsewhere; there is no buffer growth and therefore nothing to reclaim.
 */
template<typename Type, std::size_t Capacity = 4096u>
requires (std::is_trivially_copyable_v<Type> && std::has_single_bit(Capacity))
class work_stealing_deque : public utility::noncopyable {

  inline static constexpr auto mask = Capacity - 1u;

public:

  work_stealing_deque() = default;

  /** @brief Owner only. False when the deque is full. */
  auto push(const Type& value) -> bool {
    const auto bottom = _bottom.load(std::memory_order_relaxed);
    const auto top = _top.load(std::memory_order_acquire);

    if (bottom - top >= static_cast<std::int64_t>(Capacity)) {
      return false;
    }

    _buffer[_slot(bottom)].store(value, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    _bottom.store(bottom + 1, std::memory_order_relaxed);

    return true;
  }

  /** @brief Owner only. The most recently pushed value, unless empty or a thief took the last one. */
  auto pop() -> std::optional<Type> {
    const auto bottom = _bottom.load(std::memory_order_relaxed) - 1;
    _bottom.store(bottom, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto top = _top.load(std::memory_order_relaxed);

    if (top > bottom) {
      _bottom.store(bottom + 1, std::memory_order_relaxed);
      return std::nullopt;
    }

    const auto value = _buffer[_slot(bottom)].load(std::memory_order_relaxed);

    if (top != bottom) {
      return value;
    }

    // The last element: race the thieves for it.
    const auto won = _top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
    _bottom.store(bottom + 1, std::memory_order_relaxed);

    return won ? std::optional<Type>{value} : std::nullopt;
  }

  /** @brief Any thread. The oldest value, unless empty or another thread took it first. */
  auto steal() -> std::optional<Type> {
    auto top = _top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const auto bottom = _bottom.load(std::memory_order_acquire);

    if (top >= bottom) {
      return std::nullopt;
    }

    const auto value = _buffer[_slot(top)].load(std::memory_order_relaxed);

    if (!_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
      return std::nullopt;
    }

    return value;
  }

  /** @brief A racy hint, exact only on the owner with no thieves running. */
  [[nodiscard]] auto is_empty() const noexcept -> bool {
    return _bottom.load(std::memory_order_relaxed) <= _top.load(std::memory_order_relaxed);
  }

private:

  [[nodiscard]] static constexpr auto _slot(std::int64_t index) noexcept -> std::size_t {
    return static_cast<std::size_t>(index) & mask;
  }

  // Thieves hammer top, the owner bottom: separate cache lines.
  alignas(memory::cacheline::size) std::atomic<std::int64_t> _top{0};
  alignas(memory::cacheline::size) std::atomic<std::int64_t> _bottom{0};
  alignas(memory::cacheline::size) std::array<std::atomic<Type>, Capacity> _buffer{};

}; // class work_stealing_deque

} // namespace sbx::async

#endif // LIBSBX_ASYNC_WORK_STEALING_DEQUE_HPP_
