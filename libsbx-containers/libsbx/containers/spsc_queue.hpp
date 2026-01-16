#ifndef LIBSBX_CONTAINERS_SPSC_QUEUE_HPP_
#define LIBSBX_CONTAINERS_SPSC_QUEUE_HPP_

#include <atomic>

#include <libsbx/utility/fast_mod.hpp>

#include <libsbx/memory/cache.hpp>

namespace sbx::containers {

template<typename Type, std::size_t Capacity>
class spsc_queue {

public:

  ~spsc_queue() {
    auto tmp = Type{};

    while (pop(tmp)) {

    }
  }

  bool push(const Type& value) {
    const auto head = _head.load(std::memory_order_relaxed);
    const auto next = increment(head);

    if (next == _tail.load(std::memory_order_acquire)) {
      return false; // full
    }

    auto* slot = _slot(head);
    std::construct_at(slot, value);

    _head.store(next, std::memory_order_release);

    return true;
  }

  bool pop(Type& out) {
    const auto tail = _tail.load(std::memory_order_relaxed);

    if (tail == _head.load(std::memory_order_acquire)) {
      return false;
    }

    auto* element = _slot(tail);
    out = std::move(*element);
    std::destroy_at(element);

    _tail.store(increment(tail), std::memory_order_release);

    return true;
  }

private:

  static constexpr auto increment(const std::size_t i) -> std::size_t {
    return utility::fast_mod(i + 1, Capacity);
  }

  auto _slot(const std::size_t index) noexcept -> Type* {
    return std::launder(reinterpret_cast<Type*>(_buffer.data()) + index);
  }

  auto _slot(const std::size_t index) const noexcept -> const Type* {
    return std::launder(reinterpret_cast<const Type*>(_buffer.data()) + index);
  }

  alignas(memory::cacheline::size) std::atomic<std::size_t> _head{0};
  alignas(memory::cacheline::size) std::atomic<std::size_t> _tail{0};
  alignas(Type) std::array<std::byte, Capacity * sizeof(Type)> _buffer{};

}; // class spsc_queue

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_SPSC_QUEUE_HPP_