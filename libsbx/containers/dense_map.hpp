// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_DENSE_MAP_HPP_
#define LIBSBX_CONTAINERS_DENSE_MAP_HPP_

#include <memory>
#include <vector>
#include <cmath>
#include <bit>

#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/assert.hpp>

#include <libsbx/memory/iterable_adaptor.hpp>
#include <libsbx/memory/concepts.hpp>

#include <libsbx/containers/compressed_pair.hpp>

namespace sbx::containers {

namespace detail {

template<typename Key, typename Type>
struct dense_map_node final {

  using value_type = std::pair<Key, Type>;
  using size_type = std::size_t;

  template<typename... Args>
  dense_map_node(const size_type position, Args&&... args)
  : element{std::forward<Args>(args)...},
    next{position} { }

  template<typename Allocator, typename... Args>
  dense_map_node(std::allocator_arg_t, const Allocator& allocator, const size_type position, Args&&... args)
  : element{std::make_obj_using_allocator<value_type>(allocator, std::forward<Args>(args)...)},
    next{position} { }

  template<typename Allocator>
  dense_map_node(std::allocator_arg_t, const Allocator& allocator, const dense_map_node& other)
  : element{std::make_obj_using_allocator<value_type>(allocator, other.element)},
    next{other.next} { }

  template<typename Allocator>
  dense_map_node(std::allocator_arg_t, const Allocator& allocator, dense_map_node&& other)
  : element{std::make_obj_using_allocator<value_type>(allocator, std::move(other.element))},
    next{other.next} { }

  value_type element;
  size_type next;

}; // struct dense_map_node

template<typename Iterator>
class dense_map_iterator final {

  template<typename>
  friend class dense_map_iterator;

  template<typename Lhs, typename Rhs>
  friend constexpr auto operator-(const dense_map_iterator<Lhs>&, const dense_map_iterator<Rhs>&) noexcept -> std::ptrdiff_t;

  template<typename Lhs, typename Rhs>
  friend constexpr auto operator==(const dense_map_iterator<Lhs>&, const dense_map_iterator<Rhs>&) noexcept -> bool;

  template<typename Lhs, typename Rhs>
  friend constexpr auto operator<=>(const dense_map_iterator<Lhs>&, const dense_map_iterator<Rhs>&) noexcept -> std::strong_ordering;

  using first_type = decltype(std::as_const(std::declval<Iterator>()->element.first));
  using second_type = decltype((std::declval<Iterator>()->element.second));

public:

  using value_type = std::pair<first_type, second_type>;
  using pointer = memory::input_iterator_pointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using iterator_concept = std::random_access_iterator_tag;

  constexpr dense_map_iterator() noexcept
  : _iterator{} { }

  constexpr dense_map_iterator(const Iterator iterator) noexcept
  : _iterator{iterator} {}

  template<typename Other, typename = std::enable_if_t<!std::is_same_v<Iterator, Other> && std::is_constructible_v<Iterator, Other>>>
  constexpr dense_map_iterator(const dense_map_iterator<Other>& other) noexcept
  : _iterator{other._iterator} {}

  constexpr auto operator++() noexcept -> dense_map_iterator& {
    ++_iterator;
    return *this;
  }

  constexpr auto operator++(int) noexcept -> dense_map_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  constexpr auto operator--() noexcept -> dense_map_iterator& {
    --_iterator;
    return *this;
  }

  constexpr auto operator--(int) noexcept -> dense_map_iterator {
    const auto original = *this;
    --(*this);
    return original;
  }

  constexpr auto operator+=(const difference_type value) noexcept -> dense_map_iterator& {
    _iterator += value;
    return *this;
  }

  constexpr auto operator+(const difference_type value) const noexcept -> dense_map_iterator {
    auto copy = *this;
    return (copy += value);
  }

  constexpr auto operator-=(const difference_type value) noexcept -> dense_map_iterator& {
    return (*this += -value);
  }

  constexpr auto operator-(const difference_type value) const noexcept -> dense_map_iterator {
    return (*this + -value);
  }

  [[nodiscard]] constexpr auto operator[](const difference_type value) const noexcept -> reference {
    return {_iterator[value].element.first, _iterator[value].element.second};
  }

  [[nodiscard]] constexpr auto operator->() const noexcept -> pointer {
    return operator*();
  }

  [[nodiscard]] constexpr auto operator*() const noexcept -> reference {
    return operator[](0);
  }

private:

  Iterator _iterator;

}; // struct dense_map_iterator

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator-(const dense_map_iterator<Lhs>& lhs, const dense_map_iterator<Rhs>& rhs) noexcept -> std::ptrdiff_t {
  return lhs._iterator - rhs._iterator;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator==(const dense_map_iterator<Lhs>& lhs, const dense_map_iterator<Rhs>& rhs) noexcept -> bool {
  return lhs._iterator == rhs._iterator;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator<=>(const dense_map_iterator<Lhs>& lhs, const dense_map_iterator<Rhs>& rhs) noexcept -> std::strong_ordering {
  return lhs._iterator <=> rhs._iterator;
}

template<typename Iterator>
class dense_map_local_iterator final {

  template<typename>
  friend class dense_map_local_iterator;

  using first_type = decltype(std::as_const(std::declval<Iterator>()->element.first));
  using second_type = decltype((std::declval<Iterator>()->element.second));

public:

  using value_type = std::pair<first_type, second_type>;
  using size_type = std::size_t;
  using pointer = memory::input_iterator_pointer<value_type>;
  using reference = value_type;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;

  constexpr dense_map_local_iterator() noexcept
  : _iterator{},
    _offset{} { }

  constexpr dense_map_local_iterator(Iterator iterator, const std::size_t position) noexcept
  : _iterator{iterator},
    _offset{position} {}

  template<typename Other, typename = std::enable_if_t<!std::is_same_v<Iterator, Other> && std::is_constructible_v<Iterator, Other>>>
  constexpr dense_map_local_iterator(const dense_map_local_iterator<Other>& other) noexcept
  : _iterator{other._iterator},
    _offset{other._offset} {}

  constexpr auto operator++() noexcept -> dense_map_local_iterator& {
    return (_offset = _iterator[static_cast<typename Iterator::difference_type>(_offset)].next), *this;
  }

  constexpr auto operator++(int) noexcept -> dense_map_local_iterator {
    const auto original = *this;
    ++(*this);
    return original;
  }

  [[nodiscard]] constexpr auto operator->() const noexcept -> pointer {
    return operator*();
  }

  [[nodiscard]] constexpr auto operator*() const noexcept -> reference {
    const auto index = static_cast<typename Iterator::difference_type>(_offset);
    return {_iterator[index].element.first, _iterator[index].element.second};
  }

  [[nodiscard]] constexpr auto index() const noexcept -> size_type {
    return _offset;
  }

private:

  Iterator _iterator;
  size_type _offset;

}; // struct dense_map_local_iterator

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto operator==(const dense_map_local_iterator<Lhs>& lhs, const dense_map_local_iterator<Rhs>& rhs) noexcept -> bool{
  return lhs.index() == rhs.index();
}

} // namespace detail

template<typename Key, typename Type, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>, typename Allocator = std::allocator<std::pair<Key, Type>>>
class dense_map {

  static constexpr auto default_threshold = std::float_t{0.875f};
  static constexpr auto minimum_capacity = std::size_t{8};

  using node_type = detail::dense_map_node<Key, Type>;
  using allocator_traits = std::allocator_traits<Allocator>;

  using sparse_container_type = std::vector<std::size_t, memory::rebound_allocator_t<Allocator, std::size_t>>;
  using dense_container_type = std::vector<node_type, memory::rebound_allocator_t<Allocator, node_type>>;

public:

  using allocator_type = Allocator;
  using value_type = std::pair<Key, Type>;
  using key_type = Key;
  using mapped_type = Type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using iterator = detail::dense_map_iterator<typename dense_container_type::iterator>;
  using const_iterator = detail::dense_map_iterator<typename dense_container_type::const_iterator>;
  using local_iterator = detail::dense_map_local_iterator<typename dense_container_type::iterator>;
  using const_local_iterator = detail::dense_map_local_iterator<typename dense_container_type::const_iterator>;

  dense_map()
  : dense_map{minimum_capacity} { }

  explicit dense_map(const allocator_type& allocator)
  : dense_map{minimum_capacity, hasher{}, key_equal{}, allocator} { }

  dense_map(const size_type count, const allocator_type& allocator)
  : dense_map{count, hasher{}, key_equal{}, allocator} { }

  dense_map(const size_type count, const hasher& hash, const allocator_type& allocator)
  : dense_map{count, hash, key_equal{}, allocator} { }

  explicit dense_map(const size_type count, const hasher& hash = hasher{}, const key_equal& equal = key_equal{}, const allocator_type& allocator = allocator_type{})
  : _sparse{allocator, hash},
    _dense{allocator, equal},
    _threshold{default_threshold} {
    rehash(count);
  }

  dense_map(const dense_map& other) = default;

  dense_map(const dense_map& other, const allocator_type& allocator)
  : _sparse{std::piecewise_construct, std::forward_as_tuple(other._sparse.first(), allocator), std::forward_as_tuple(other._sparse.second())},
    _dense{std::piecewise_construct, std::forward_as_tuple(other._dense.first(), allocator), std::forward_as_tuple(other._dense.second())},
    _threshold{other._threshold} { }

  dense_map(dense_map&& other) noexcept = default;

  dense_map(dense_map&& other, const allocator_type& allocator)
  : _sparse{std::piecewise_construct, std::forward_as_tuple(std::move(other._sparse.first()), allocator), std::forward_as_tuple(std::move(other._sparse.second()))},
    _dense{std::piecewise_construct, std::forward_as_tuple(std::move(other._dense.first()), allocator), std::forward_as_tuple(std::move(other._dense.second()))},
    _threshold{other._threshold} { }

  ~dense_map() = default;

  auto operator=(const dense_map& other) -> dense_map& = default;

  auto operator=(dense_map&& other) noexcept -> dense_map& = default;

  auto swap(dense_map& other) noexcept -> void {
    using std::swap;
    swap(_sparse, other._sparse);
    swap(_dense, other._dense);
    swap(_threshold, other._threshold);
  }

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type {
    return _sparse.first().get_allocator();
  }

  [[nodiscard]] auto cbegin() const noexcept -> const_iterator {
    return _dense.first().begin();
  }

  [[nodiscard]] auto begin() const noexcept -> const_iterator {
    return cbegin();
  }

  [[nodiscard]] auto begin() noexcept -> iterator {
    return _dense.first().begin();
  }

  [[nodiscard]] auto cend() const noexcept -> const_iterator {
    auto& first = _dense.first();
    return first.end();
  }

  [[nodiscard]] auto end() const noexcept -> const_iterator {
    return cend();
  }

  [[nodiscard]] auto end() noexcept -> iterator {
    return _dense.first().end();
  }

  [[nodiscard]] bool empty() const noexcept {
    return _dense.first().empty();
  }

  [[nodiscard]] auto size() const noexcept -> size_type {
    return _dense.first().size();
  }

  [[nodiscard]] auto max_size() const noexcept -> size_type {
    return _dense.first().max_size();
  }

  auto clear() noexcept -> void {
    _sparse.first().clear();
    _dense.first().clear();
    rehash(0u);
  }

  auto insert(const value_type& value) -> std::pair<iterator, bool> {
    return _insert_or_do_nothing(value.first, value.second);
 }

  auto insert(value_type&& value) -> std::pair<iterator, bool>  {
    return _insert_or_do_nothing(std::move(value.first), std::move(value.second));
  }

  template<typename Arg>
  requires(std::is_constructible_v<value_type, Arg&&>)
  auto insert(Arg&& value) -> std::pair<iterator, bool> {
    return _insert_or_do_nothing(std::forward<Arg>(value).first, std::forward<Arg>(value).second);
  }

  template<typename... Args>
  auto emplace([[maybe_unused]] Args&&... args) -> std::pair<iterator, bool> {
    if constexpr (sizeof...(Args) == 0u) {
      return _insert_or_do_nothing(key_type{});
    } else if constexpr (sizeof...(Args) == 1u) {
        return _insert_or_do_nothing(std::forward<Args>(args).first..., std::forward<Args>(args).second...);
    } else if constexpr (sizeof...(Args) == 2u) {
      return _insert_or_do_nothing(std::forward<Args>(args)...);
    } else {
      auto& node = _dense.first().emplace_back(_dense.first().size(), std::forward<Args>(args)...);
      const auto index = _key_to_bucket(node.element.first);

      if (auto it = _constrained_find(node.element.first, index); it != end()) {
        _dense.first().pop_back();
        return std::make_pair(it, false);
      }

      std::swap(node.next, _sparse.first()[index]);
      _rehash_if_required();

      return std::make_pair(--end(), true);
    }
  }

  auto erase(const_iterator position) -> iterator {
    const auto offset = position - cbegin();
    erase(position->first);
    return begin() + offset;
  }

  auto erase(const key_type& key) -> bool {
    for (auto* current = &_sparse.first()[key_to_bucket(key)]; *current != (std::numeric_limits<size_type>::max)(); current = &_dense.first()[*current].next) {
      if (_dense.second()(_dense.first()[*current].element.first, key)) {
        const auto index = *current;
        *current = _dense.first()[*current].next;
        _move_and_pop(index);

        return true;
      }
    }

    return false;
  }

  [[nodiscard]] auto at(const key_type& key) -> mapped_type& {
    auto it = find(key);
    utility::assert_that(it != end(), "Invalid key");

    return it->second;
  }

  /*! @copydoc at */
  [[nodiscard]] auto at(const key_type &key) const -> const mapped_type& {
    auto it = find(key);
    utility::assert_that(it != cend(), "Invalid key");
    return it->second;
  }

  [[nodiscard]] auto operator[](const key_type& key) -> mapped_type& {
    return _insert_or_do_nothing(key).first->second;
  }

  [[nodiscard]] auto operator[](key_type&& key) -> mapped_type& {
    return _insert_or_do_nothing(std::move(key)).first->second;
  }

  [[nodiscard]] auto find(const key_type& key) -> iterator {
    return _constrained_find(key, _key_to_bucket(key));
  }

  [[nodiscard]] auto find(const key_type& key) const -> const_iterator {
    return _constrained_find(key, _key_to_bucket(key));
  }

  [[nodiscard]] auto contains(const key_type& key) const -> bool {
    return find(key) != cend();
  }

  [[nodiscard]] auto cbegin(const size_type index) const -> const_local_iterator {
    return {_dense.first().begin(), _sparse.first()[index]};
  }

  [[nodiscard]] auto begin(const size_type index) const -> const_local_iterator {
    return cbegin(index);
  }

  [[nodiscard]] auto begin(const size_type index) -> local_iterator {
    return {_dense.first().begin(), _sparse.first()[index]};
  }

  [[nodiscard]] auto cend([[maybe_unused]] const size_type index) const -> const_local_iterator {
    return {_dense.first().begin(), (std::numeric_limits<size_type>::max)()};
  }

  [[nodiscard]] auto end(const size_type index) const -> const_local_iterator {
    return cend(index);
  }

  [[nodiscard]] auto end([[maybe_unused]] const size_type index) -> local_iterator {
    return {_dense.first().begin(), (std::numeric_limits<size_type>::max)()};
  }

  [[nodiscard]] auto max_load_factor() const -> std::float_t {
    return _threshold;
  }

  auto set_max_load_factor(const std::float_t value) -> void {
    utility::assert_that(value > 0.f, "Invalid load factor");
    _threshold = value;
    rehash(0u);
  }

  [[nodiscard]] auto bucket_count() const -> size_type {
    return _sparse.first().size();
  }

  void rehash(const size_type count) {
    auto value = count > minimum_capacity ? count : minimum_capacity;
    const auto capacity = static_cast<size_type>(static_cast<std::float_t>(size()) / max_load_factor());
    value = value > capacity ? value : capacity;

    if (const auto length = std::bit_ceil(value); length != bucket_count()) {
      _sparse.first().resize(length);

      for (auto&& elem: _sparse.first()) {
        elem = (std::numeric_limits<size_type>::max)();
      }

      for (auto position = 0u, last = size(); position < last; ++position) {
        const auto index = _key_to_bucket(_dense.first()[position].element.first);
        _dense.first()[position].next = std::exchange(_sparse.first()[index], position);
      }
    }
  }

  void reserve(const size_type count) {
    _dense.first().reserve(count);
    rehash(static_cast<size_type>(std::ceil(static_cast<std::float_t>(count) / max_load_factor())));
  }

  [[nodiscard]] auto hash_function() const -> hasher {
    return _sparse.second();
  }

  [[nodiscard]] auto key_eq() const -> key_equal {
    return _dense.second();
  }

private:

  template<typename Other>
  [[nodiscard]] auto _key_to_bucket(const Other& key) const noexcept -> size_type {
    return utility::fast_mod(static_cast<size_type>(_sparse.second()(key)), bucket_count());
  }

  template<typename Other>
  [[nodiscard]] auto _constrained_find(const Other& key, const size_type bucket) -> iterator {
    for (auto it = begin(bucket), last = end(bucket); it != last; ++it) {
      if (_dense.second()(it->first, key)) {
        return begin() + static_cast<difference_type>(it.index());
      }
    }

    return end();
  }

  template<typename Other>
  [[nodiscard]] auto _constrained_find(const Other& key, const size_type bucket) const -> const_iterator {
    for (auto it = cbegin(bucket), last = cend(bucket); it != last; ++it) {
      if (_dense.second()(it->first, key)) {
        return cbegin() + static_cast<difference_type>(it.index());
      }
    }

    return cend();
  }

  template<typename Other, typename... Args>
  [[nodiscard]] auto _insert_or_do_nothing(Other &&key, Args &&...args) -> std::pair<iterator, bool> {
    const auto index = _key_to_bucket(key);

    if (auto it = _constrained_find(key, index); it != end()) {
      return std::make_pair(it, false);
    }

    _dense.first().emplace_back(_sparse.first()[index], std::piecewise_construct, std::forward_as_tuple(std::forward<Other>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
    _sparse.first()[index] = _dense.first().size() - 1u;
    _rehash_if_required();

    return std::make_pair(--end(), true);
  }

  template<typename Other, typename Arg>
  [[nodiscard]] auto _insert_or_overwrite(Other&& key, Arg&& value) -> std::pair<iterator, bool> {
    const auto index = key_to_bucket(key);

    if (auto it = _constrained_find(key, index); it != end()) {
      it->second = std::forward<Arg>(value);
      return std::make_pair(it, false);
    }

    _dense.first().emplace_back(_sparse.first()[index], std::forward<Other>(key), std::forward<Arg>(value));
    _sparse.first()[index] = _dense.first().size() - 1u;
    _rehash_if_required();

    return std::make_pair(--end(), true);
  }

  auto _move_and_pop(const size_type position) -> void {
    if (const auto last = size() - 1u; position != last) {
      auto* current = &_sparse.first()[_key_to_bucket(_dense.first().back().element.first)];
      _dense.first()[position] = std::move(_dense.first().back());
      for (; *current != last; current = &_dense.first()[*current].next) {}
      *current = position;
    }

    _dense.first().pop_back();
  }

  auto _rehash_if_required() -> void {
    if (const auto count = bucket_count(); size() > static_cast<size_type>(static_cast<std::float_t>(count) * max_load_factor())) {
      rehash(count * 2u);
    }
  }

  compressed_pair<sparse_container_type, hasher> _sparse;
  compressed_pair<dense_container_type, key_equal> _dense;
  std::float_t _threshold;

}; // class dense_map

} // namespace sbx::containers

template<typename Key, typename Value, typename Allocator>
struct std::uses_allocator<sbx::containers::detail::dense_map_node<Key, Value>, Allocator> : std::true_type { };

#endif // LIBSBX_CONTAINERS_DENSE_MAP_HPP_
