// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_RESOURCE_REGISTRY_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_RESOURCE_REGISTRY_HPP_

#include <cstdint>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/type_list.hpp>
#include <libsbx/utility/concepts.hpp>

#include <libsbx/graphics/resources/resource_handle.hpp>
#include <libsbx/graphics/resources/resource_pool.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

namespace sbx::graphics {

/**
 * @brief One @ref resource_pool per resource type, selected at compile time.
 *
 * @ref collect_all is called once per frame from `frame_context::begin_frame`, after the timeline
 * wait and before any descriptor writes, so a slot is never reused while still referenced.
 *
 * Guarded by a single mutex against concurrent render/asset-loading threads. Resource storage is
 * page-stable, so a reference from @ref get stays valid after the lock is released.
 *
 * @tparam Types The resource types the registry owns. Must be unique.
 */
template<typename... Types>
requires (sizeof...(Types) > 0u && utility::are_unique_v<Types...>)
class basic_resource_registry : public utility::noncopyable {

  using type_list = utility::type_list<Types...>;

public:

  basic_resource_registry() = default;

  ~basic_resource_registry() = default;

  template<typename Type>
  requires (utility::type_list_contains_v<Type, type_list>)
  [[nodiscard]] auto pool() noexcept -> resource_pool<Type>& {
    return std::get<utility::type_list_index_v<Type, type_list>>(_pools);
  }

  template<typename Type>
  requires (utility::type_list_contains_v<Type, type_list>)
  [[nodiscard]] auto pool() const noexcept -> const resource_pool<Type>& {
    return std::get<utility::type_list_index_v<Type, type_list>>(_pools);
  }

  template<typename Type, typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  auto emplace(Args&&... args) -> resource_handle<Type> {
    auto lock = std::lock_guard{_mutex};

    return pool<Type>().emplace(std::forward<Args>(args)...);
  }

  template<typename Type>
  [[nodiscard]] auto get(const resource_handle<Type> handle) -> Type& {
    auto lock = std::lock_guard{_mutex};

    return pool<Type>().get(handle);
  }

  template<typename Type>
  [[nodiscard]] auto get(const resource_handle<Type> handle) const -> const Type& {
    auto lock = std::lock_guard{_mutex};

    return pool<Type>().get(handle);
  }

  template<typename Type>
  [[nodiscard]] auto is_valid(const resource_handle<Type> handle) const noexcept -> bool {
    auto lock = std::lock_guard{_mutex};

    return pool<Type>().is_valid(handle);
  }

  /**
   * @brief Marks a resource unreachable and schedules its destruction for @p timeline_value.
   *
   * Pass `frame_context::frame_index()`. Conservative but safe: the resource can only have been
   * referenced by frame N or earlier when frame N retires it.
   */
  template<typename Type>
  auto retire(const resource_handle<Type> handle, const std::uint64_t timeline_value) -> void {
    auto lock = std::lock_guard{_mutex};

    pool<Type>().retire(handle, timeline_value);
  }

  /**
   * @brief Destroys everything retired at or before @p completed_value across every pool.
   *
   * @param completed_value The highest timeline value the GPU has signalled.
   */
  auto collect_all(const std::uint64_t completed_value) -> void {
    auto lock = std::lock_guard{_mutex};

    std::apply([completed_value](auto&... pools) -> void {
      (pools.collect(completed_value), ...);
    }, _pools);
  }

  /**
   * @brief Destroys everything every pool owns, retired or not.
   *
   * Only safe once the device is idle.
   */
  auto clear_all() -> void {
    auto lock = std::lock_guard{_mutex};

    std::apply([](auto&... pools) -> void {
      (pools.clear(), ...);
    }, _pools);
  }

  /**
   * @brief The number of resources across all pools still waiting on the timeline.
   */
  [[nodiscard]] auto pending_count() const noexcept -> std::size_t {
    auto lock = std::lock_guard{_mutex};

    auto result = std::size_t{0u};

    std::apply([&result](const auto&... pools) -> void {
      ((result += pools.pending_count()), ...);
    }, _pools);

    return result;
  }

private:

  mutable std::mutex _mutex{};

  std::tuple<resource_pool<Types>...> _pools{};

}; // class basic_resource_registry

using resource_registry = basic_resource_registry<buffer, image>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RESOURCES_RESOURCE_REGISTRY_HPP_
