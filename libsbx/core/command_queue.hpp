#ifndef LIBSBX_CORE_COMMAND_QUEUE_HPP_
#define LIBSBX_CORE_COMMAND_QUEUE_HPP_

#include <memory>
#include <functional>

#include <libsbx/utility/function_ptr.hpp>

#include <libsbx/memory/aligned_byte_buffer.hpp>
#include <libsbx/memory/buffer_cursor.hpp>
#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/units.hpp>

namespace sbx::core {

template<std::size_t Size>
class command_queue {

  using command_function_ptr = utility::function_ptr<void, sbx::memory::observer_ptr<std::byte>>;

  struct command_header {
    command_function_ptr function;
    std::uint32_t size;
    std::uint32_t reserved;
  }; // struct command_header

  static_assert(sizeof(command_header) == 16);
  static_assert(alignof(command_header) >= alignof(std::uint64_t));

  static constexpr auto alignment = std::size_t{alignof(std::max_align_t)};

public:

  static constexpr auto size = Size;

  command_queue() 
  : _buffer{sbx::memory::make_aligned_buffer(size, alignment)},
    _cursor{_buffer.get()},
    _count{0u} { }

  template<typename Callable>
  auto enqueue(Callable&& callable) -> void {
    using callable_type = std::remove_reference_t<Callable>;

    auto command = [](sbx::memory::observer_ptr<std::byte> ptr) {
      auto* function = reinterpret_cast<callable_type*>(ptr.get());

      std::invoke(*function);

      if constexpr (!std::is_trivially_destructible_v<callable_type>) {
        std::destroy_at(function);
      }
    };

    auto storage = _allocate(command, sizeof(callable_type));
    std::construct_at(reinterpret_cast<callable_type*>(storage.get()), std::forward<Callable>(callable));
  }

  auto execute() -> void {
    auto cursor = sbx::memory::buffer_cursor<std::byte>{_buffer.get()};

    for (auto i = std::uint32_t{0}; i < _count; ++i) {
      auto* header = reinterpret_cast<command_header*>(cursor.get());

      cursor += sizeof(command_header);

      std::invoke(header->function, cursor.get());

      cursor += header->size;
    }

    _cursor.reset(_buffer.get());
    _count = 0;
  }

private:

  auto _allocate(command_function_ptr function, const std::uint32_t size) -> sbx::memory::observer_ptr<std::byte> {
    const auto aligned_size = static_cast<std::uint32_t>(memory::align_up(size, alignment));

    auto* header = reinterpret_cast<command_header*>(_cursor.get());

    header->function = function;
    header->size = aligned_size;
    header->reserved = 0;

    _cursor += sizeof(command_header);

    auto* memory = _cursor.get();

    _cursor += aligned_size;

    ++_count;

    return memory;
  }

  sbx::memory::aligned_byte_buffer _buffer;
  sbx::memory::buffer_cursor<std::byte> _cursor;
  std::size_t _count;

}; // class command_queue

} // namespace sbx::core

#endif // LIBSBX_CORE_COMMAND_QUEUE_HPP_