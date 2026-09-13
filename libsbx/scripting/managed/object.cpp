// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/managed/object.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

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
  if (!_validate_handle("set field", name)) {
    return;
  }

  auto field_name = string::create(name);

  std::invoke(detail::backend.set_field_value, _handle, field_name, value);

  string::destroy(field_name);
}

void object::get_field_value_raw(std::string_view name, void* value) const {
  if (!_validate_handle("get field", name)) {
    return;
  }

  auto field_name = string::create(name);

  std::invoke(detail::backend.get_field_value, _handle, field_name, value);

  string::destroy(field_name);
}

void object::set_property_value_raw(std::string_view name, const void* value) const {
  if (!_validate_handle("set property", name)) {
    return;
  }

  auto property_name = string::create(name);

  std::invoke(detail::backend.set_property_value, _handle, property_name, value);

  string::destroy(property_name);
}

void object::get_property_value_raw(std::string_view name, void* value) const {
  if (!_validate_handle("get property", name)) {
    return;
  }

  auto property_name = string::create(name);

  std::invoke(detail::backend.get_property_value, _handle, property_name, value);

  string::destroy(property_name);
}

auto object::get_type() const -> const type& {
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
  if (!_validate_handle("invoke method", name)) {
    return;
  }

  const auto& owner_type = get_type();

  auto handle = std::int32_t{-1};

  if (const auto entry = owner_type._method_handles.find(name); entry != owner_type._method_handles.end()) {
    handle = entry->second;
  } else {
    auto method_name = string::create(name);
    handle = std::invoke(detail::backend.get_method_handle, _handle, method_name, parameter_types, static_cast<std::int32_t>(length));
    string::destroy(method_name);

    owner_type._method_handles.emplace(std::string{name}, handle);
  }

  if (handle < 0) {
    return;
  }

  std::invoke(detail::backend.invoke_method_handle, _handle, handle, parameters, static_cast<std::int32_t>(length));
}

void object::_invoke_method_return_internal(std::string_view name, const void** parameters, const managed_type* parameter_types, std::size_t length, void* result_storage) const {
  if (!_validate_handle("invoke method", name)) {
    return;
  }

  const auto& owner_type = get_type();

  auto handle = std::int32_t{-1};

  if (const auto entry = owner_type._method_handles.find(name); entry != owner_type._method_handles.end()) {
    handle = entry->second;
  } else {
    auto method_name = string::create(name);
    handle = std::invoke(detail::backend.get_method_handle, _handle, method_name, parameter_types, static_cast<std::int32_t>(length));
    string::destroy(method_name);

    owner_type._method_handles.emplace(std::string{name}, handle);
  }

  if (handle < 0) {
    return;
  }

  std::invoke(detail::backend.invoke_method_handle_return, _handle, handle, parameters, static_cast<std::int32_t>(length), result_storage);
}

auto object::_validate_handle(std::string_view message, std::string_view name) const noexcept -> bool {
  // Must run in every build configuration, not just debug: this is the only thing standing between
  // a null/invalid handle (a failed construction, or a use after destroy()) and forwarding it
  // straight into the CLR host via GCHandle.FromIntPtr(...).Target on the C# side.
  if (!_handle) {
    if constexpr (utility::is_build_type_debug_v) {
      utility::logger<"scripting">::error("Cannot {} '{}' on an object with no handle (construction likely failed — see the earlier C# log/exception)", message, name);
    }

    return false;
  }

  return true;
}

}; // namespace sbx::scripting::managed
