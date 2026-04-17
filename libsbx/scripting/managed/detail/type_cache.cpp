// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed::detail {

auto type_cache::get() -> type_cache& {
  static auto instance = type_cache{};

  return instance;
}

auto type_cache::cache_type(type&& value) -> type* {
  auto* type = &_types.insert(std::move(value)).second;
  _name_cache[type->get_full_name()] = type;
  _id_cache[type->get_type_id()] = type;

  return type;
}

auto type_cache::get_type_by_name(std::string_view name) const -> type* {
  if (auto entry = _name_cache.find(std::string{name}); entry != _name_cache.end()) {
    return entry->second;
  }

  return nullptr;
}

auto type_cache::get_type_by_id(type_id id) const -> type* {
  if (auto entry = _id_cache.find(id); entry != _id_cache.end()) {
    return entry->second;
  }

  return nullptr;
}

auto type_cache::clear() -> void {
  _types.clear();
  _name_cache.clear();
  _id_cache.clear();
}

} // namespace sbx::scripting::managed::detail