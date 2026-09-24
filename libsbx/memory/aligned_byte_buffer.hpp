// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_
#define LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_

#include <cstddef>
#include <memory>
#include <new>

namespace sbx::memory {

/** @brief Deleter for aligned_byte_buffer; frees storage allocated with the alignment it was constructed with. */
struct aligned_byte_deleter {

  std::size_t alignment{};

  auto operator()(std::byte* ptr) const noexcept -> void {
    ::operator delete(ptr, std::align_val_t{alignment});
  }

}; // struct aligned_byte_deleter

/** @brief An over-aligned heap byte buffer, freed with the alignment it was allocated with. */
using aligned_byte_buffer = std::unique_ptr<std::byte[], aligned_byte_deleter>;

/**
 * @brief Allocates an aligned_byte_buffer.
 *
 * @param size The number of bytes to allocate.
 * @param alignment The required alignment; must be a valid alignment (a power of two).
 *
 * @return The allocated buffer.
 */
inline auto make_aligned_buffer(const std::size_t size, const std::size_t alignment) -> aligned_byte_buffer {
  auto* memory = static_cast<std::byte*>(::operator new(size, std::align_val_t{alignment}));

  return aligned_byte_buffer{memory, aligned_byte_deleter{alignment}};
}

/** @return value rounded up to the next multiple of alignment. alignment must be a power of two. */
constexpr auto align_up(const std::size_t value, const std::size_t alignment) noexcept -> std::size_t {
  return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ALIGNED_BYTE_BUFFER_HPP_
