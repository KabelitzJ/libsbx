// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_OBJECT_HPP_
#define LIBSBX_SCRIPTING_MANAGED_OBJECT_HPP_

#include <cmath>

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <libsbx/scripting/managed/fwd.hpp>
#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>

namespace sbx::scripting::managed {

enum class managed_type : std::uint8_t {
  unknown,
  int8,
  uint8,
  int16,
  uint16,
  int32,
  uint32,
  int64,
  uint64,
  float32,
  float64,
  boolean,
  string,
  pointer
}; // enum class managed_type 

template<typename Type>
constexpr auto get_managed_type() -> managed_type {
  if constexpr (std::same_as<Type, std::uint8_t> || std::same_as<Type, std::byte>) {
    return managed_type::uint8;
  } else if constexpr (std::same_as<Type, std::uint16_t>) {
    return managed_type::uint16;
  } else if constexpr (std::same_as<Type, std::uint32_t> || (std::same_as<Type, unsigned long> && sizeof(Type) == 4u)) {
    return managed_type::uint32;
  } else if constexpr (std::same_as<Type, std::uint64_t> || (std::same_as<Type, unsigned long> && sizeof(Type) == 8u)) {
    return managed_type::uint64;
  } else if constexpr (std::same_as<Type, char8_t> || std::same_as<Type, std::int8_t>) {
    return managed_type::int8;
  } else if constexpr (std::same_as<Type, std::int16_t>) {
    return managed_type::int16;
  } else if constexpr (std::same_as<Type, std::int32_t> || (std::same_as<Type, long> && sizeof(Type) == 4u)) {
    return managed_type::int32;
  } else if constexpr (std::same_as<Type, std::int64_t> || (std::same_as<Type, long> && sizeof(Type) == 8u)) {
    return managed_type::int64;
  } else if constexpr (std::same_as<Type, std::float_t>) {
    return managed_type::float32;
  } else if constexpr (std::same_as<Type, std::double_t>) {
    return managed_type::float64;
  } else if constexpr (std::same_as<Type, bool>) {
    return managed_type::boolean;
  } else if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, managed::string>) {
    return managed_type::string;
  } else {
    return managed_type::unknown;
  }
}

namespace detail {

template<std::size_t Size, typename Tuple, std::size_t Index>
inline auto add_to_array(std::array<const void*, Size>& arguments, std::array<managed_type, Size>& types, const Tuple& tuple) -> void {
  using T = std::tuple_element_t<Index, Tuple>;

  types[Index] = get_managed_type<std::remove_reference_t<T>>();

  if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
    arguments[Index] = static_cast<const void*>(std::get<Index>(tuple));
  } else {
    arguments[Index] = static_cast<const void*>(&std::get<Index>(tuple));
  }
}

} // namespace detail

template<std::size_t Size, typename Tuple, std::size_t... Indices>
inline auto add_to_array(std::array<const void*, Size>& arguments, std::array<managed_type, Size>& types, const Tuple& tuple, std::index_sequence<Indices...>) -> void {
  (detail::add_to_array<Size, Tuple, Indices>(arguments, types, tuple), ...);
}

class object {

  friend class assembly;
  friend class type;

public:

  template<typename Result, typename... Args>
  auto invoke(std::string_view name, Args&&... args) const -> Result {
    constexpr auto parameter_count = sizeof...(args);

    auto result = Result{};
    
    if constexpr (parameter_count > 0u) {
      auto storage = std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...};

      auto parameter_values = std::array<const void*, parameter_count>{};
      auto parameter_types = std::array<managed_type, parameter_count>{};

      add_to_array(parameter_values, parameter_types, storage, std::make_index_sequence<parameter_count>{});

      _invoke_method_return_internal(name, parameter_values.data(), parameter_types.data(), parameter_count, &result);
    } else {
      _invoke_method_return_internal(name, nullptr, nullptr, 0, &result);
    }

    return result;
  }

  template<typename... Args>
  auto invoke(std::string_view name, Args&&... args) const -> void {
    constexpr auto parameter_count = sizeof...(args);

    if constexpr (parameter_count > 0u) {
      auto storage = std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...};

      auto parameter_values = std::array<const void*, parameter_count>{};
      auto parameter_types = std::array<managed_type, parameter_count>{};

      add_to_array(parameter_values, parameter_types, storage, std::make_index_sequence<parameter_count>{});

      _invoke_method_internal(name, parameter_values.data(), parameter_types.data(), parameter_count);
    } else {
      _invoke_method_internal(name, nullptr, nullptr, 0);
    }
  }

  template<typename Type>
  auto set_field_value(std::string_view name, const Type& value) const -> void {
    set_field_value_raw(name, &value);
  }

  template<typename Type>
  auto get_field_value(std::string_view name) const -> Type {
    auto result = Type{};

    get_field_value_raw(name, &result);

    return result;
  }

  template<typename TValue>
  auto set_property_value(std::string_view name, TValue value) const -> void {
    set_property_value_raw(name, &value);
  }

  template<typename Result>
  auto get_property_value(std::string_view name) const -> Result
  {
    Result result;
    get_property_value_raw(name, &result);
    return result;
  }

  auto set_field_value_raw(std::string_view name, const void* value) const -> void;

  auto get_field_value_raw(std::string_view name, void* value) const -> void;

  auto set_property_value_raw(std::string_view name, const void* value) const -> void;

  auto get_property_value_raw(std::string_view name, void* value) const -> void;

  auto get_type() -> const type&;
  
  auto destroy() -> void;

  auto is_valid() const -> bool;

private:

  auto _invoke_method_internal(std::string_view name, const void** parameters, const managed_type* parameter_types, std::size_t length) const -> void;

  auto _invoke_method_return_internal(std::string_view name, const void** parameters, const managed_type* parameter_types, std::size_t length, void* result_storage) const -> void;

  void* _handle;
  const type* _type;

private:
  
}; // class object

template<>
auto object::set_field_value<std::string>(std::string_view name, const std::string& value) const -> void;

template<>
auto object::set_field_value<bool>(std::string_view name, const bool& value) const -> void;

template<>
auto object::get_field_value<std::string>(std::string_view name) const -> std::string;

template<>
auto object::get_field_value<bool>(std::string_view name) const -> bool;

}; // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_OBJECT_HPP_
