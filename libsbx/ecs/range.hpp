// SPDX-License-Identifier: MIT
#ifndef LIBSBX_RANGE_HPP_
#define LIBSBX_RANGE_HPP_

#include <cmath>
#include <iterator>
#include <type_traits>
#include <concepts>

#include <libsbx/utility/iterator.hpp>

namespace sbx::ecs {

namespace detail {

template<std::integral Type>
struct range_iterator_base : utility::iterator<std::input_iterator_tag, Type> {
  
  range_iterator_base(Type current) : _current(current) { }

  auto operator*() const -> Type { 
    return _current; 
  }

  auto operator->() const -> const Type* { 
    return &_current; 
  }

  auto operator++() -> range_iterator_base& {
    ++_current;
    return *this;
  }

  auto operator++(int) -> range_iterator_base {
    auto copy = *this;
    ++(*this);
    return copy;
  }

  auto operator==(const range_iterator_base& other) const -> bool {
    return _current == other._current;
  }

protected:

  Type _current;

}; // struct range_iterator_base

} // namespace detail

template<std::integral Type>
struct step_range_proxy {
  struct iterator : detail::range_iterator_base<Type> {
    iterator(Type current, Type step)
    : detail::range_iterator_base<Type>{current}, 
      _step{step} { }

    using detail::range_iterator_base<Type>::_current;

    auto operator++() -> iterator& {
      _current += _step;
      return *this;
    }

    auto operator ++(int) -> iterator {
      auto copy = *this;
      ++(*this);
      return copy;
    }

    auto operator ==(iterator const& other) const -> bool {
      return _step > 0 ? _current >= other._current : _current < other._current;
    }

    Type _step;
  }; // struct iterator

  step_range_proxy(Type begin, Type end, Type step)
  : _begin{begin, step}, 
    _end{end, step} { }

  auto begin() const -> iterator { 
    return _begin; 
  }

  auto end() const -> iterator { 
    return _end; 
  }

  auto size() const -> std::size_t { 
    if (*_end >= *_begin) {
      // Increasing and empty range
      if (_begin._step < Type{0}) {
        return 0;
      }
    } else {
      // Decreasing range
      if (_begin._step > Type{0}) {
        return 0;
      }
    }
    return std::ceil(std::abs(static_cast<std::float_t>(*_end - *_begin) / _begin._step));
  }

private:

  iterator _begin;
  iterator _end;

}; // struct step_range_proxy

template<std::integral Type>
struct range_proxy {
  struct iterator : detail::range_iterator_base<Type> {
    iterator(Type current) 
    : detail::range_iterator_base<Type>{current} { }
  }; // iterator

  range_proxy(Type begin, Type end) 
  : _begin{begin}, 
    _end{end} { }

  auto step(Type step) -> step_range_proxy<Type> {
    return {*_begin, *_end, step};
  }

  auto begin() const -> iterator { 
    return _begin; 
  }

  auto end() const -> iterator { 
    return _end; 
  }

  auto size() const -> std::size_t { 
    return *_end - *_begin; 
  }

private:

  iterator _begin;
  iterator _end;

}; // struct range_proxy

template<std::integral Type>
struct step_infinite_range_proxy {
  struct iterator : detail::range_iterator_base<Type> {
    iterator(Type current = Type{}, Type step = Type{})
    : detail::range_iterator_base<Type>{current}, 
      _step{step} { }

    using detail::range_iterator_base<Type>::_current;

    auto operator++() -> iterator& {
      _current += _step;
      return *this;
    }

    auto operator++(int) -> iterator {
      auto copy = *this;
      ++(*this);
      return copy;
    }

    auto operator==(const iterator&) const -> bool { 
      return false; 
    }

  private:

    Type _step;

  }; // iterator

  step_infinite_range_proxy(Type begin, Type step)
  : _begin{begin, step} { }

  auto begin() const -> iterator { 
    return _begin; 
  }

  auto end() const -> iterator { 
    return  iterator{}; 
  }

private:

  iterator _begin;

}; // step_infinite_range_proxy

template<std::integral Type>
struct infinite_range_proxy {
  struct iterator : detail::range_iterator_base<Type> {
    iterator(Type current = Type{}) 
    : detail::range_iterator_base<Type>{current} { }

    bool operator==(const iterator&) const { 
      return false; 
    }

  }; // struct iterator

  infinite_range_proxy(Type begin)
  : _begin{begin} { }

  auto step(Type step) -> step_infinite_range_proxy<Type> {
    return {*_begin, step};
  }

  auto begin() const -> iterator { 
    return _begin; 
  }

  auto end() const -> iterator { 
    return iterator{}; 
  }

private:

  iterator _begin;

}; // struct infinite_range_proxy

template<std::integral T, std::integral U>
auto range(T begin, U end) -> range_proxy<std::common_type_t<T, U>> {
  using common_type = std::common_type_t<T, U>;
  return range_proxy{static_cast<common_type>(begin), static_cast<common_type>(end)};
}

template <typename Type>
auto range(Type begin) -> infinite_range_proxy<Type> {
  return infinite_range_proxy{begin};
}

template<typename Container>
requires (requires(const Container& container){ { container.size() } -> std::integral; })
auto indices(const Container& container) -> range_proxy<decltype(container.size())> {
  return range_proxy{0, container.size()};
}

template<typename Type, std::size_t Size>
auto indices(Type(&)[Size]) -> range_proxy<std::size_t> {
  return range_proxy{std::size_t{0}, Size};
}

template<typename Type>
auto indices(std::initializer_list<Type>&& container) -> range_proxy<typename std::initializer_list<Type>::size_type> {
  return range_proxy{std::size_t{0}, container.size()};
}

} // namespace sbx::ecs

#endif // LIBSBX_RANGE_HPP_
