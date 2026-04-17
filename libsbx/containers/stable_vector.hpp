// SPDX-License-Identifier: MIT
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
    

  stable_vector(const stable_vector& other) {
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

  auto operator[](const size_type index) -> reference {
    const auto page_index = index / page_size;

    return _page_table[page_index]->elements[index - (page_index * page_size)];
  }

  auto operator[](const size_type index) const -> const_reference {
    const auto page_index = index / page_size;

    return _page_table[page_index]->elements[index - (page_index * page_size)];
  }

  auto get_element_count() const -> size_type { 
    return _element_count; 
  }

  auto insert(value_type&& element) -> std::pair<std::uint32_t, reference> {
    const auto page_index = _element_count / page_size;

    if (_element_count >= _capacity) {
      auto lock = std::scoped_lock{_mutex};

      if (_element_count >= _capacity) {
        auto* new_page = new page();

        if (page_index >= _page_count) {
          auto old_pages = _page_count;

          _page_count = (std::max)(std::uint64_t(16), _page_count * 2);

          auto new_page_table = std::make_unique<page*[]>(_page_count);

          std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

          _page_table.exchange(new_page_table.get());
          _page_tables.push_back(std::move(new_page_table));
        }

        _page_table[page_index] = new_page;

        _capacity += page_size;
      }
    }

    std::uint32_t index = (++_element_count - 1);
    _page_table[page_index]->elements[index - (page_index * page_size)] = std::move(element);
    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  auto insert_no_lock(value_type&& element) -> std::pair<std::uint32_t, reference> {
    const auto page_index = _element_count / page_size;

    if (_element_count >= _capacity) {
      auto lock = std::scoped_lock{_mutex};

      if (_element_count >= _capacity) {
        auto* new_page = new page();

        if (page_index >= _page_count) {
          auto old_pages = _page_count;

          _page_count = (std::max)(std::uint64_t(16), _page_count * 2);

          auto new_page_table = std::make_unique<page*[]>(_page_count);

          std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

          _page_table.exchange(new_page_table.get());
          _page_tables.push_back(std::move(new_page_table));
        }

        _page_table[page_index] = new_page;

        _capacity += page_size;
      }
    }

    const auto index = (++_element_count - 1);

    _page_table[page_index]->elements[index - (page_index * page_size)] = std::move(element);

    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  auto emplace_back() -> std::pair<std::uint32_t, reference> {
    const auto page_index = _element_count / page_size;

    if (_element_count >= _capacity) {
      auto* new_page = new page();

      if (page_index >= _page_count) {
        auto old_pages = _page_count;

        _page_count = (std::max)(std::uint64_t(16), _page_count * 2);

        auto new_page_table = std::make_unique<page*[]>(_page_count);

        std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

        _page_table.exchange(new_page_table.get());
        _page_tables.push_back(std::move(new_page_table));
      }

      _page_table[page_index] = new_page;

      _capacity += page_size;
    }

    const auto index = (++_element_count - 1);

    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  auto emplace_back_no_lock() -> std::pair<std::uint32_t, reference> {
    const auto page_index = _element_count / page_size;

    if (_element_count >= _capacity) {
      auto* new_page = new page();

      if (page_index >= _page_count) {
        auto old_pages = _page_count;

        _page_count = std::max(std::uint64_t{16}, _page_count * 2u);

        auto new_page_table = std::make_unique<page*[]>(_page_count);

        std::memcpy(new_page_table.get(), _page_table.load(), old_pages * pointer_size);

        _page_table.exchange(new_page_table.get());
        _page_tables.push_back(std::move(new_page_table));
      }

      _page_table[page_index] = new_page;

      _capacity += page_size;
    }

    const auto index = (++_element_count - 1);

    return { index, _page_table[page_index]->elements[index - (page_index * page_size)] };
  }

  template<typename Callable>
  requires (std::is_invocable_v<Callable, reference>)
  auto for_each(Callable&& callable) -> void {
    for (auto i = 0u; i < _element_count; ++i) {
      std::invoke(callable, (*this)[i]);
    }
  }

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

  std::shared_mutex _mutex;
  std::list<std::unique_ptr<page*[]>> _page_tables;

  std::atomic<page**> _page_table;
  std::atomic<std::uint32_t> _element_count;
  std::atomic<std::uint32_t> _capacity;
  std::uint64_t _page_count;

}; // class stable_vector

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_STABLE_VECTOR_HPP_