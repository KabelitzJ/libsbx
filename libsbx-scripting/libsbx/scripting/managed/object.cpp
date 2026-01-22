// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/object.hpp>

#include <libsbx/scripting/managed/detail/type_cache.hpp>
#include <libsbx/scripting/managed/detail/backend.hpp>

namespace sbx::scripting::managed {

template<>
auto object::set_field_value<std::string>(std::string_view name, const std::string& value) const -> void {
  auto string = string::create(value);

  set_field_value_raw(name, &string);

  string::destroy(string);
}

template<>
auto object::set_field_value<bool>(std::string_view name, const bool& value) const -> void {
  auto boolean = bool32{value};

  set_field_value_raw(name, &boolean);
}

template<>
auto object::get_field_value<std::string>(std::string_view name) const -> std::string {
  auto result = string{};

  get_field_value_raw(name, &result);

  auto string = std::string{result};

  string::destroy(result);

  return string;
}

template<>
auto object::get_field_value<bool>(std::string_view name) const -> bool {
  auto result = bool32{};

  get_field_value_raw(name, &result);

  return result;
}

void object::set_field_value_raw(std::string_view name, const void* value) const {
  auto field_name = string::create(name);

  std::invoke(detail::backend.set_field_value, _handle, field_name, value);

  string::destroy(field_name);
}

void object::get_field_value_raw(std::string_view name, void* value) const {
  auto field_name = string::create(name);

  std::invoke(detail::backend.get_field_value, _handle, field_name, value);

  string::destroy(field_name);
}

void object::set_property_value_raw(std::string_view name, const void* value) const {
  auto property_name = string::create(name);

  std::invoke(detail::backend.set_property_value, _handle, property_name, value);

  string::destroy(property_name);
}

void object::get_property_value_raw(std::string_view name, void* value) const {
  auto property_name = string::create(name);

  std::invoke(detail::backend.get_property_value, _handle, property_name, value);

  string::destroy(property_name);
}

auto object::get_type() -> const type& {
  if (!_type) {
    auto new_type = type{};

    std::invoke(detail::backend.get_object_type_id, _handle, &new_type._id);

    _type = detail::type_cache::get().cache_type(std::move(new_type));
  }

  return *_type;
}

void object::destroy() {
  if (!_handle) {
    return;
  }

  std::invoke(detail::backend.destroy_object, _handle);

  _handle = nullptr;
  _type = nullptr;
}

auto object::is_valid() const -> bool { 
  return _handle != nullptr && _type != nullptr; 
}

void object::_invoke_method_internal(std::string_view name, const void** parameters, const managed_type* parameter_types, std::size_t length) const {
  auto method_name = string::create(name);

  std::invoke(detail::backend.invoke_method, _handle, method_name, parameters, parameter_types, static_cast<std::int32_t>(length));

  string::destroy(method_name);
}

void object::_invoke_method_return_internal(std::string_view name, const void** parameters, const managed_type* parameter_types, std::size_t length, void* result_storage) const {
  auto method_name = string::create(name);

  std::invoke(detail::backend.invoke_method_return, _handle, method_name, parameters, parameter_types, static_cast<std::int32_t>(length), result_storage);

  string::destroy(method_name);
}


}; // namespace sbx::scripting::managed