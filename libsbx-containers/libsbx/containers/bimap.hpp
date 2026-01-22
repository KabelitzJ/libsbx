// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_BIMAP_HPP_
#define LIBSBX_CONTAINERS_BIMAP_HPP_

#include <unordered_map>
#include <map>
#include <optional>

namespace sbx::containers {

template<template<typename, typename> typename Map, typename Key, typename Value>
class basic_bimap {

  template<typename K, typename V>
  using map_type = Map<K, V>;

  using forward_map_type = map_type<Key, Value>;
  using reverse_map_type = map_type<Value, Key>;

public:

  using key_type = Key;
  using value_type = Value;

  auto insert(const Key& key, const Value& value) -> void {
    _forward.insert({key, value});
    _reverse.insert({value, key});
  }

  auto erase_key(const key_type& key) -> void {
    _reverse.erase(_forward[key]);
    _forward.erase(key);
  }

  auto erase_value(const value_type& value) -> void {
    _forward.erase(_reverse[value]);
    _reverse.erase(value);
  }

  auto find_key(const value_type& value) -> std::optional<std::reference_wrapper<key_type>> {
    if (auto entry = _reverse.find(value); entry != _reverse.end()) {
      return std::ref(entry->second);
    }

    return std::nullopt;
  }

  auto find_value(const key_type& key) -> std::optional<std::reference_wrapper<value_type>> {
    if (auto entry = _forward.find(key); entry != _forward.end()) {
      return std::ref(entry->second);
    }

    return std::nullopt;
  }

private:

  forward_map_type _forward;
  reverse_map_type _reverse;

}; // class bimap

template<typename Key, typename Value>
using unordered_bimap = basic_bimap<std::unordered_map, Key, Value>;

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_BIMAP_HPP_
