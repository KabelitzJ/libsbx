#ifndef LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_
#define LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_

#include <utility>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory_resource>

namespace sbx::memory {

template<typename Key, typename Value, std::size_t Capacity, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
class local_unordered_map {

  using base_type = std::pmr::unordered_map<Key, Value, Hash, EqualTo>;

  // [TODO] KAJ 2025-12-16 : This is a rough estimate
  inline static constexpr auto buffer_size = std::size_t{Capacity * (sizeof(typename base_type::value_type) + sizeof(void*) * 2) + Capacity * sizeof(void*)};

public:

  using key_type = base_type::key_type;
  using mapped_type = base_type::mapped_type;
  using value_type = base_type::value_type;
  using size_type = base_type::size_type;
  using iterator = base_type::iterator;

  explicit local_unordered_map(std::pmr::memory_resource* upstream = std::pmr::null_memory_resource())
  : _memory{},
    _resource{_memory.data(), _memory.size(), upstream},
    _base{&_resource} {
    _base.reserve(Capacity);
  }

  auto operator[](const key_type& key) -> mapped_type& {
    return _base[key];
  }

  auto operator[](key_type&& key) -> mapped_type& {
    return _base[std::move(key)];
  }

  template<typename... Args>
  auto try_emplace(const key_type& key, Args&&... args) -> std::pair<iterator, bool> {
    return _base.try_emplace(key, std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto try_emplace(key_type&& key, Args&&... args) -> std::pair<iterator, bool> {
    return _base.try_emplace(std::move(key), std::forward<Args>(args)...);
  }

private:

  alignas(std::max_align_t) std::array<std::uint8_t, buffer_size> _memory;
  std::pmr::monotonic_buffer_resource _resource;
  base_type _base;

}; // class stack_unordered_map

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_LOCAL_CONTAINERS_HPP_