// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/containers/stable_vector.hpp
 *
 * @brief A paged, thread-safe vector that never invalidates references to existing elements on growth.
 *
 * @ingroup libsbx-containers
 */

#ifndef LIBSBX_CONTAINERS_STABLE_VECTOR_HPP_
#define LIBSBX_CONTAINERS_STABLE_VECTOR_HPP_

#include <cstring>

#include <array>
#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace sbx::containers {

inline constexpr auto pointer_size = sizeof(void*);

/**
 * @brief A vector allocated in fixed-size pages instead of one contiguous buffer, so growing it never reallocates (and so never invalidates) existing elements — unlike std::vector, a reference returned by insert()/emplace_back() stays valid for the object's lifetime. Insertion is lock-free on the fast path (an atomic claim into already-allocated pages) and only takes _mutex when a new page needs allocating.
 *
 * @tparam Type The element type.
 * @tparam PageSize The number of elements per page.
 */
template<typename Type, std::size_t PageSize = 256>
class stable_vector {

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using size_type = std::size_t;

  inline static constexpr auto page_size = PageSize;

  stable_vector()
  : _page_table{nullptr},
    _element_count{0},
    _capacity{0},
    _page_count{0} { }

  stable_vector(const stable_vector& other)
  : _page_table{nullptr},
    _element_count{0},
    _capacity{0},
    _page_count{0} {
    other.for_each([this](const_reference element) mutable {
      emplace_back_no_lock().second = element;
    });
  }

  ~stable_vector() {
    for (auto i = 0u; i < _page_count; i++) {
      delete _page_table[i];
    }
  }

  auto operator=(const stable_vector& other) -> stable_vector& {
    if (this == &other) {
      return *this;
    }

    clear();

    other.for_each([this](const_reference element) mutable {
      emplace_back_no_lock().second = element;
    });

    return *this;
  }

  /** @brief Discards every element and frees all pages. */
  auto clear() -> void {
    for (auto i = 0u; i < _page_count; i++) {
      delete _page_table[i];
    }

    _element_count = 0;
    _capacity = 0;
    _page_count = 0;

    _page_tables.clear();
    _page_table.store(nullptr);
  }

  /** @brief Unchecked element access; index must be < get_element_count(). */
  auto operator[](const size_type index) -> reference {
    const auto page_index = index / page_size;

    return _page_table[page_index]->elements[index - (page_index * page_size)];
  }

  /** @copydoc operator[] */
  auto operator[](const size_type index) const -> const_reference {
    const auto page_index = index / page_size;

    return _page_table[page_index]->elements[index - (page_index * page_size)];
  }

  auto get_element_count() const -> size_type {
    return _element_count;
  }

  /**
   * @brief Claims a slot (thread-safe) and move-assigns element into it.
   *
   * @param element The value to move in.
   *
   * @return The new element's index and a reference to it.
   */
  auto insert(value_type&& element) -> std::pair<std::uint32_t, reference> {
    const auto [index, page_index] = _claim_slot();

    _page_table[page_index]->elements[index - (page_index * page_size)] = std::move(element);
    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  /** @brief Same as insert(), without taking _mutex — the caller is responsible for external synchronization. */
  auto insert_no_lock(value_type&& element) -> std::pair<std::uint32_t, reference> {
    const auto [index, page_index] = _claim_slot();

    _page_table[page_index]->elements[index - (page_index * page_size)] = std::move(element);
    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  /**
   * @brief Claims a slot (thread-safe) without initializing it — the returned reference is to a
   * default-constructed Type the caller is expected to assign into.
   *
   * @return The new element's index and a reference to it.
   */
  auto emplace_back() -> std::pair<std::uint32_t, reference> {
    const auto [index, page_index] = _claim_slot();

    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  /** @brief Same as emplace_back(), without taking _mutex — the caller is responsible for external synchronization. */
  auto emplace_back_no_lock() -> std::pair<std::uint32_t, reference> {
    const auto [index, page_index] = _claim_slot_no_lock();

    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  /**
   * @brief Invokes callable(reference) for every live element, in index order.
   *
   * @tparam Callable A callable invocable with (reference).
   *
   * @param callable Invoked once per element.
   */
  template<typename Callable>
  requires (std::is_invocable_v<Callable, reference>)
  auto for_each(Callable&& callable) -> void {
    for (auto i = 0u; i < _element_count; ++i) {
      std::invoke(callable, (*this)[i]);
    }
  }

  /** @copydoc for_each */
  template<typename Callable>
  requires (std::is_invocable_v<Callable, const_reference>)
  auto for_each(Callable&& callable) const -> void {
    for (auto i = 0u; i < _element_count; ++i) {
      std::invoke(callable, (*this)[i]);
    }
  }

private:

  struct page {
    std::array<value_type, page_size> elements;
  }; // struct page

  /**
   * @brief Reserves the next slot and grows the page table (under _mutex) to cover it if needed, returning {index, page_index}. index is claimed via an atomic fetch-add *before* page_index is derived from it — deriving page_index from a pre-increment read of _element_count instead (as this used to do) lets a concurrent claim crossing a page boundary between that read and the increment land index in a different page than page_index names, writing past the end of that page's std::array. The growth check is a `while`, not an `if`, so a claim landing far beyond the current capacity (e.g. this thread lost a long race) grows one page at a time until covered.
   */
  auto _claim_slot() -> std::pair<std::uint32_t, std::uint64_t> {
    const auto index = _element_count.fetch_add(1);
    const auto page_index = static_cast<std::uint64_t>(index) / page_size;

    if (index >= _capacity) {
      auto lock = std::scoped_lock{_mutex};

      while (index >= _capacity) {
        auto* new_page = new page();
        const auto new_page_index = static_cast<std::uint64_t>(_capacity.load()) / page_size;

        if (new_page_index >= _page_count) {
          const auto old_pages = _page_count;

          _page_count = (std::max)(std::uint64_t(16), _page_count * 2);

          auto new_page_table = std::make_unique<page*[]>(_page_count);

          std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

          _page_table.exchange(new_page_table.get());
          _page_tables.push_back(std::move(new_page_table));
        }

        _page_table[new_page_index] = new_page;

        _capacity += page_size;
      }
    }

    return { index, page_index };
  }

  /** @brief Same claim/grow logic as _claim_slot, without taking _mutex — the caller is responsible for external synchronization (see emplace_back_no_lock). */
  auto _claim_slot_no_lock() -> std::pair<std::uint32_t, std::uint64_t> {
    const auto index = _element_count.fetch_add(1);
    const auto page_index = static_cast<std::uint64_t>(index) / page_size;

    while (index >= _capacity) {
      auto* new_page = new page();
      const auto new_page_index = static_cast<std::uint64_t>(_capacity.load()) / page_size;

      if (new_page_index >= _page_count) {
        const auto old_pages = _page_count;

        _page_count = (std::max)(std::uint64_t(16), _page_count * 2);

        auto new_page_table = std::make_unique<page*[]>(_page_count);

        std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

        _page_table.exchange(new_page_table.get());
        _page_tables.push_back(std::move(new_page_table));
      }

      _page_table[new_page_index] = new_page;

      _capacity += page_size;
    }

    return { index, page_index };
  }

  std::shared_mutex _mutex;
  std::list<std::unique_ptr<page*[]>> _page_tables;

  std::atomic<page**> _page_table;
  std::atomic<std::uint32_t> _element_count;
  std::atomic<std::uint32_t> _capacity;
  std::uint64_t _page_count;

}; // class stable_vector

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_STABLE_VECTOR_HPP_
