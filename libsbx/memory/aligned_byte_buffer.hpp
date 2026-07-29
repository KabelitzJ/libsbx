#ifndef LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_
#define LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_

#include <memory>

namespace sbx::memory {

struct aligned_byte_deleter {

  std::size_t alignment{};

  auto operator()(std::byte* ptr) const noexcept -> void {
    ::operator delete(ptr, std::align_val_t{alignment});
  }

}; // struct aligned_byte_deleter

using aligned_byte_buffer = std::unique_ptr<std::byte[], aligned_byte_deleter>;


auto make_aligned_buffer(const std::size_t size, const std::size_t alignment) -> aligned_byte_buffer {
  auto* memory = static_cast<std::byte*>(::operator new(size, std::align_val_t{alignment}));

  return aligned_byte_buffer{memory, aligned_byte_deleter{alignment}};
}

constexpr auto align_up(const std::size_t value, const std::size_t alignment) noexcept -> std::size_t {
  return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_