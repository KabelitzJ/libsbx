// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/attribute.hpp>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed {

auto attribute::get_type() -> type& {
  if (!_type) {
    auto new_type = type{};

    std::invoke(detail::backend.get_attribute_type, _handle, &new_type._id);

    _type = detail::type_cache::get().cache_type(std::move(new_type));
  }

  return *_type;
}

auto attribute::_get_field_value_internal(std::string_view field_name, void* value) const -> void {
  auto name = string::create(field_name);

  std::invoke(detail::backend.get_attribute_field_value, _handle, name, value);

  string::destroy(name);
}

template<>
auto attribute::get_field_value<std::string>(std::string_view field_name) -> std::string {
  auto result = string{};

  _get_field_value_internal(field_name, &result);

  return std::string{result};
}

template<>
auto attribute::get_field_value<bool>(std::string_view field_name) -> bool {
  auto result = bool32{};

  _get_field_value_internal(field_name, &result);

  return result;
}

}; // namespace sbx::scripting::managed