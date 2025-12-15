#ifndef LIBSBX_MEMORY_MONOTONIC_ARENA_HPP_
#define LIBSBX_MEMORY_MONOTONIC_ARENA_HPP_

namespace sbx::memory {

class monotonic_arena {

public:

  using pointer = std::uint8_t*;

  monotonic_arena(pointer buffer, std::size_t size)
  : _buffer{buffer},
    _size{size},
    _offset{0} { }

  auto allocate(std::size_t count, std::size_t align) -> pointer {
    const auto current = reinterpret_cast<std::size_t>(_buffer + _offset);
    const auto aligned = (current + align - 1) & ~(align - 1);
    const auto new_offset = aligned - reinterpret_cast<std::size_t>(_buffer) + count;

    if (new_offset > _size) {
      throw std::bad_alloc{};
    }

    _offset = new_offset;

    return reinterpret_cast<std::uint8_t*>(aligned);
  }

private:

  pointer _buffer;
  std::size_t _size;
  std::size_t _offset;

}; // class monotonic_arena

template<typename Type, std::size_t Size>
class monotonic_arena_allocator {

  template<typename T, std::size_t S>
  friend class monotonic_arena_allocator;

public:

  using value_type = Type;
  using pointer = value_type*;

  template<typename Other>
  struct rebind {
    using other = monotonic_arena_allocator<Other, Size>;
  }; // struct rebind

  monotonic_arena_allocator(std::span<std::uint8_t, Size> buffer) noexcept 
  : _buffer{buffer},
    _arena(_buffer.data(), _buffer.size()) { }

  template<typename Other>
  monotonic_arena_allocator(const monotonic_arena_allocator<Other, Size>& other) noexcept
  : _buffer{other._buffer},
    _arena(other._arena) {}

  auto allocate(std::size_t count) -> pointer {
    return static_cast<pointer>(_arena.allocate(sizeof(value_type) * count, alignof(value_type)));
  }

  auto deallocate(pointer, std::size_t) noexcept -> void {
    // no-op (monotonic)
  }

private:

  std::span<std::uint8_t, Size> _buffer;
  monotonic_arena _arena;

}; // class monotonic_arena_allocator

template<typename Key, typename Value, std::size_t Size>
using monotonic_arena_map_allocator = monotonic_arena_allocator<std::pair<const Key, Value>, Size>;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_MONOTONIC_ARENA_HPP_