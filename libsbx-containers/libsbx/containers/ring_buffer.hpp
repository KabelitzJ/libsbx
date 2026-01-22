// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_RING_BUFFER_HPP_
#define LIBSBX_CONTAINERS_RING_BUFFER_HPP_

#include <memory>
#include <utility>

namespace sbx::containers {

template<typename Type, std::size_t Capacity>
class ring_buffer {

public:

  using value_type = Type;

  ring_buffer() noexcept
  : _head{0},
    _tail{0} { }

  ~ring_buffer() noexcept {
    for (auto i = _head; i != _tail; i = (i + 1) % Capacity) {
      auto* ptr = _ptr_at(i);
      std::destroy_at(ptr);
      ptr = nullptr;
    }
  }

  auto push(Type&& value) noexcept -> void {
    auto* ptr = _ptr_at(_head);

    if (ptr != nullptr) {
      std::destroy_at(ptr);
      ptr = nullptr;
    }

    std::construct_at(_ptr_at(_head), std::move(value));

    _head = (_head + 1) % Capacity;
  }

  auto capacity() const noexcept -> std::size_t {
    return Capacity;
  }

private:

  auto _ptr_at(std::size_t i) -> Type* {
    return reinterpret_cast<Type*>(_buffer + i);
  }

  alignas(alignof(Type)) std::byte _buffer[sizeof(Type) * Capacity];
  std::size_t _head;
  std::size_t _tail;

}; // class ring_buffer

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_RING_BUFFER_HPP_
