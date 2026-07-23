// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_RESOURCE_POOL_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_RESOURCE_POOL_HPP_

#include <array>
#include <bit>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/aligned_storage.hpp>

#include <libsbx/graphics/resources/resource_handle.hpp>

namespace sbx::graphics {

/**
 * @brief Owns resources of a single type in slots that never move.
 *
 * Slot indices are handed out through @ref resource_handle and are reused only after the GPU has
 * caught up, which makes an index safe to publish to the bindless descriptor table for the whole
 * lifetime of the resource it points at.
 *
 * Destruction is deferred: @ref retire marks a slot unreachable and records the timeline value the
 * GPU must reach before the resource may be destroyed, and @ref collect performs the destruction
 * once that value has been signalled. This is the only destruction path, so no resource is ever
 * torn down while it is still referenced by an in flight command buffer.
 *
 * @tparam Type The resource type. Needs neither a default constructor nor a move constructor.
 * @tparam PageSize The number of slots per allocation.
 */
template<typename Type, std::size_t PageSize = 64u>
requires (PageSize > 0u && std::has_single_bit(PageSize))
class resource_pool : public utility::noncopyable {

public:

  using value_type = Type;
  using handle_type = resource_handle<value_type>;
  using size_type = std::uint32_t;

  inline static constexpr auto page_size = PageSize;

  resource_pool() = default;

  ~resource_pool() {
    clear();
  }

  /**
   * @brief Constructs a resource in a free slot and returns a handle to it.
   */
  template<typename... Args>
  requires (std::is_constructible_v<value_type, Args...>)
  auto emplace(Args&&... args) -> handle_type {
    const auto index = _acquire_index();

    std::construct_at(_pointer(index), std::forward<Args>(args)...);

    auto& slot = _slots[index];

    slot.is_alive = true;

    ++_live_count;

    return handle_type{index, slot.generation};
  }

  [[nodiscard]] auto get(const handle_type handle) -> value_type& {
    utility::assert_that(is_valid(handle), "Tried to access a resource through an invalid handle");

    return *_pointer(handle.index());
  }

  [[nodiscard]] auto get(const handle_type handle) const -> const value_type& {
    utility::assert_that(is_valid(handle), "Tried to access a resource through an invalid handle");

    return *_pointer(handle.index());
  }

  [[nodiscard]] auto is_valid(const handle_type handle) const noexcept -> bool {
    if (!handle.is_valid() || handle.index() >= slot_count()) {
      return false;
    }

    const auto& slot = _slots[handle.index()];

    return slot.is_alive && slot.generation == handle.generation();
  }

  /**
   * @brief Marks a slot unreachable and schedules its destruction.
   *
   * The handle compares invalid from this point on, but the resource stays alive until
   * @ref collect is called with a value greater than or equal to @p timeline_value.
   *
   * @param timeline_value The timeline value the GPU must reach before the resource may be destroyed.
   */
  auto retire(const handle_type handle, const std::uint64_t timeline_value) -> void {
    utility::assert_that(is_valid(handle), "Tried to retire an invalid resource handle");
    utility::assert_that(_retired.empty() || _retired.back().timeline_value <= timeline_value, "Resources must be retired in non decreasing timeline order");

    auto& slot = _slots[handle.index()];

    slot.is_alive = false;
    slot.generation = static_cast<std::uint8_t>(slot.generation + 1u);

    --_live_count;

    _retired.push_back(retired_slot{timeline_value, handle.index()});
  }

  /**
   * @brief Destroys every resource retired at or before @p completed_value and frees its slot.
   *
   * @param completed_value The highest timeline value the GPU has signalled.
   */
  auto collect(const std::uint64_t completed_value) -> void {
    while (!_retired.empty() && _retired.front().timeline_value <= completed_value) {
      const auto index = _retired.front().index;

      _retired.pop_front();

      std::destroy_at(_pointer(index));

      _free_indices.push_back(index);
    }
  }

  /**
   * @brief Destroys everything the pool owns, retired or not, without waiting on the timeline.
   *
   * Only safe once the device is idle.
   */
  auto clear() -> void {
    const auto count = slot_count();

    for (auto index = size_type{0u}; index < count; ++index) {
      if (_slots[index].is_alive) {
        std::destroy_at(_pointer(index));
      }
    }

    for (const auto& entry : _retired) {
      std::destroy_at(_pointer(entry.index));
    }

    _slots.clear();
    _free_indices.clear();
    _retired.clear();
    _pages.clear();

    _live_count = 0u;
  }

  template<typename Callable>
  requires (std::is_invocable_v<Callable, value_type&>)
  auto for_each(Callable&& callable) -> void {
    const auto count = slot_count();

    for (auto index = size_type{0u}; index < count; ++index) {
      if (_slots[index].is_alive) {
        std::invoke(callable, *_pointer(index));
      }
    }
  }

  template<typename Callable>
  requires (std::is_invocable_v<Callable, const value_type&>)
  auto for_each(Callable&& callable) const -> void {
    const auto count = slot_count();

    for (auto index = size_type{0u}; index < count; ++index) {
      if (_slots[index].is_alive) {
        std::invoke(callable, *_pointer(index));
      }
    }
  }

  /**
   * @brief The number of reachable resources. Retired but not yet collected resources are excluded.
   */
  [[nodiscard]] auto size() const noexcept -> size_type {
    return _live_count;
  }

  [[nodiscard]] auto is_empty() const noexcept -> bool {
    return _live_count == 0u;
  }

  /**
   * @brief The number of slots the pool has handed out so far, including free ones.
   */
  [[nodiscard]] auto slot_count() const noexcept -> size_type {
    return static_cast<size_type>(_slots.size());
  }

  /**
   * @brief The number of resources waiting on the timeline before they can be destroyed.
   */
  [[nodiscard]] auto pending_count() const noexcept -> size_type {
    return static_cast<size_type>(_retired.size());
  }

private:

  struct slot {
    std::uint8_t generation{0u};
    bool is_alive{false};
  }; // struct slot

  struct retired_slot {
    std::uint64_t timeline_value;
    size_type index;
  }; // struct retired_slot

  struct page {
    std::array<memory::storage_for_t<value_type>, page_size> slots;
  }; // struct page

  auto _acquire_index() -> size_type {
    if (!_free_indices.empty()) {
      const auto index = _free_indices.back();

      _free_indices.pop_back();

      return index;
    }

    const auto index = static_cast<size_type>(_slots.size());

    utility::assert_that(index < handle_type::invalid_index, "Resource pool ran out of addressable slots");

    const auto page_index = index / page_size;

    if (page_index >= _pages.size()) {
      _pages.push_back(std::make_unique_for_overwrite<page>());
    }

    _slots.emplace_back();

    return index;
  }

  auto _pointer(const size_type index) noexcept -> value_type* {
    const auto page_index = index / page_size;
    const auto slot_index = index % page_size;

    return std::launder(reinterpret_cast<value_type*>(std::addressof(_pages[page_index]->slots[slot_index])));
  }

  auto _pointer(const size_type index) const noexcept -> const value_type* {
    const auto page_index = index / page_size;
    const auto slot_index = index % page_size;

    return std::launder(reinterpret_cast<const value_type*>(std::addressof(_pages[page_index]->slots[slot_index])));
  }

  std::vector<std::unique_ptr<page>> _pages{};
  std::vector<slot> _slots{};
  std::vector<size_type> _free_indices{};
  std::deque<retired_slot> _retired{};

  size_type _live_count{0u};

}; // class resource_pool

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RESOURCES_RESOURCE_POOL_HPP_
