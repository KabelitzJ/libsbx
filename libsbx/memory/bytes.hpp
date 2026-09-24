// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_BYTES_HPP_
#define LIBSBX_MEMORY_BYTES_HPP_

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>

namespace sbx::memory {

/**
 * @brief A read-only byte view over a single trivially copyable object — e.g. for handing a push constants struct to a graphics API call that wants `const void*` + size. Requires trivial copyability (rather than accepting any @p Type) since the returned bytes are only ever meaningful to reinterpret back into @p Type — and only trivially copyable types allow that.
 *
 * @tparam Type The type of the object to view.
 *
 * @param value The object to view.
 *
 * @return A span over @p value's own storage, sizeof(Type) bytes long.
 */
template<typename Type>
requires (std::is_trivially_copyable_v<Type>)
[[nodiscard]] auto as_bytes(const Type& value) noexcept -> std::span<const std::byte> {
  return std::span<const std::byte>{reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(Type)};
}

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_BYTES_HPP_
