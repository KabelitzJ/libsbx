// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_RESOURCE_HANDLE_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_RESOURCE_HANDLE_HPP_

#include <cstdint>
#include <functional>

#include <libsbx/utility/hash.hpp>

namespace sbx::graphics {

/**
 * @brief A generational reference to a resource owned by a @ref resource_pool.
 *
 * The index doubles as the resource's bindless descriptor slot, so it stays stable for the
 * resource's lifetime. The generation is bumped on retire, invalidating outstanding handles to
 * that slot immediately, even before the slot is reused.
 *
 * @tparam Type The resource type the handle refers to.
 */
template<typename Type>
class resource_handle {

public:

  using value_type = Type;

  inline static constexpr auto index_bits = std::uint32_t{24u};
  inline static constexpr auto generation_bits = std::uint32_t{8u};

  inline static constexpr auto invalid_index = std::uint32_t{(1u << index_bits) - 1u};
  inline static constexpr auto max_generation = std::uint32_t{(1u << generation_bits) - 1u};

  constexpr resource_handle() noexcept
  : _index{invalid_index},
    _generation{0u} { }

  constexpr resource_handle(const std::uint32_t index, const std::uint8_t generation) noexcept
  : _index{index & invalid_index},
    _generation{generation} { }

  [[nodiscard]] constexpr auto index() const noexcept -> std::uint32_t {
    return _index;
  }

  [[nodiscard]] constexpr auto generation() const noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(_generation);
  }

  [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
    return _index != invalid_index;
  }

  constexpr explicit operator bool() const noexcept {
    return is_valid();
  }

  constexpr auto operator==(const resource_handle& other) const noexcept -> bool = default;

private:

  std::uint32_t _index : index_bits;
  std::uint32_t _generation : generation_bits;

}; // class resource_handle

} // namespace sbx::graphics

template<typename Type>
struct std::hash<sbx::graphics::resource_handle<Type>> {

  auto operator()(const sbx::graphics::resource_handle<Type>& handle) const noexcept -> std::size_t {
    auto result = std::size_t{0u};

    sbx::utility::hash_combine(result, handle.index(), handle.generation());

    return result;
  }

}; // struct std::hash

#endif // LIBSBX_GRAPHICS_RESOURCES_RESOURCE_HANDLE_HPP_
