#ifndef LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_
#define LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_

#include<cstdint>
#include <utility>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory_resource>

namespace sbx::memory {

namespace detail {

template<typename Container, std::size_t Capacity>
struct buffer_size;

template<typename Key, typename Value, std::size_t Capacity, typename Hash, typename EqualTo>
struct buffer_size<std::pmr::unordered_map<Key, Value, Hash, EqualTo>, Capacity> {
  inline static constexpr auto value_type_size = sizeof(typename std::pmr::unordered_map<Key, Value, Hash, EqualTo>::value_type);
  inline static constexpr auto pointer_size = sizeof(void*);
  // [NOTE] KAJ 2025-12-16 : This is a rough estimate
  inline static constexpr auto value = Capacity * (value_type_size + pointer_size * 2) + Capacity * pointer_size;
}; // struct buffer_size

template<typename Type, std::size_t Capacity>
struct buffer_size<std::pmr::vector<Type>, Capacity> {
  inline static constexpr auto value = Capacity * sizeof(Type);
}; // struct buffer_size

template<typename Container, std::size_t Capacity>
inline constexpr auto buffer_size_v = buffer_size<Container, Capacity>::value;

} // namespace detail

template<typename Key, typename Value, std::size_t Capacity, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
class local_unordered_map final {

  using underlying_type = std::pmr::unordered_map<Key, Value, Hash, EqualTo>;

  inline static constexpr auto buffer_size = detail::buffer_size_v<underlying_type, Capacity>;

  using buffer_type = std::array<std::uint8_t, buffer_size>;
  using resource_type = std::pmr::monotonic_buffer_resource;

public:

  using key_type = typename underlying_type::key_type;
  using mapped_type = typename underlying_type::mapped_type;
  using value_type = typename underlying_type::value_type;
  using size_type = typename underlying_type::size_type;
  using iterator = typename underlying_type::iterator;
  using const_iterator = typename underlying_type::const_iterator;

  explicit local_unordered_map(std::pmr::memory_resource* upstream = std::pmr::null_memory_resource())
  : _buffer{},
    _resource{_buffer.data(), _buffer.size(), upstream},
    _underlying{&_resource} {
    _underlying.reserve(Capacity);
  }

  auto operator[](const key_type& key) -> mapped_type& {
    return _underlying[key];
  }

  auto operator[](key_type&& key) -> mapped_type& {
    return _underlying[std::move(key)];
  }

  template<typename... Args>
  auto try_emplace(const key_type& key, Args&&... args) -> std::pair<iterator, bool> {
    return _underlying.try_emplace(key, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto try_emplace(key_type&& key, Args&&... args) -> std::pair<iterator, bool> {
    return _underlying.try_emplace(std::move(key), std::forward<Args>(args)...);
  }

  auto find(const key_type& key) -> iterator {
    return _underlying.find(key);
  }

  auto find(const key_type& key) const -> const_iterator {
    return _underlying.find(key);
  }

private:

  alignas(std::max_align_t) buffer_type _buffer;
  resource_type _resource;
  underlying_type _underlying;

}; // class stack_unordered_map

template<typename Type, std::size_t Capacity>
class local_vector final {

  using underlying_type = std::pmr::vector<Type>;

  inline static constexpr auto buffer_size = detail::buffer_size_v<underlying_type, Capacity>;

  using buffer_type = std::array<std::uint8_t, buffer_size>;
  using resource_type = std::pmr::monotonic_buffer_resource;

public:

  using value_type = typename underlying_type::value_type;
  using difference_type = typename underlying_type::difference_type;
  using size_type = typename underlying_type::size_type;
  using reference = typename underlying_type::reference;
  using const_reference = typename underlying_type::const_reference;
  using pointer =  typename underlying_type::pointer;
  using const_pointer = typename underlying_type::const_pointer;
  using iterator = typename underlying_type::iterator;
  using const_iterator = typename underlying_type::const_iterator;
  using reverse_iterator = typename underlying_type::reverse_iterator;
  using const_reverse_iterator = typename underlying_type::const_reverse_iterator;

  explicit local_vector(std::pmr::memory_resource* upstream = std::pmr::null_memory_resource())
  : _buffer{},
    _resource{_buffer.data(), _buffer.size(), upstream},
    _underlying{&_resource} {
    _underlying.reserve(Capacity);
  }

  auto push_back(const Type& value) -> void {
    _underlying.push_back(value);
  }

  auto push_back(Type&& value) -> void {
    _underlying.push_back(std::move(value));
  }

  template<typename... Args>
  auto emplace_back(Args&&... args) -> reference {
    return _underlying.emplace_back(std::forward<Args>(args)...);
  }

  auto begin() -> iterator {
    return _underlying.begin();
  }

  auto begin() const -> const_iterator {
    return _underlying.begin();
  }

  auto cbegin() const -> const_iterator {
    return _underlying.cbegin();
  }

  auto rbegin() -> reverse_iterator {
    return _underlying.rbegin();
  }

  auto rbegin() const -> const_reverse_iterator {
    return _underlying.rbegin();
  }

  auto crbegin() const -> const_reverse_iterator {
    return _underlying.crbegin();
  }

  auto end() -> iterator {
    return _underlying.end();
  }

  auto end() const -> const_iterator {
    return _underlying.end();
  }

  auto cend() const -> const_iterator {
    return _underlying.cend();
  }

  auto rend() -> reverse_iterator {
    return _underlying.rend();
  }

  auto rend() const -> const_reverse_iterator {
    return _underlying.rend();
  }

  auto crend() const -> const_reverse_iterator {
    return _underlying.crend();
  }

  auto size() const -> size_type {
    return _underlying.size();
  }

  auto operator[](size_type index) -> reference {
    return _underlying[index];
  }

  auto operator[](size_type index) const -> const_reference {
    return _underlying[index];
  }

  auto at(size_type index) -> reference {
    return _underlying.at(index);
  }

  auto at(size_type index) const -> const_reference {
    return _underlying.at(index);
  }

  auto front() -> reference {
    return _underlying.front();
  }

  auto front() const -> const_reference {
    return _underlying.front();
  }

  auto back() -> reference {
    return _underlying.back();
  }

  auto back() const -> const_reference {
    return _underlying.back();
  }

private:

  alignas(std::max_align_t) buffer_type _buffer;
  resource_type _resource;
  underlying_type _underlying;

}; // class local_vector

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_
