// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_ATRIBUTE_HPP_
#define LIBSBX_SCRIPTING_MANAGED_ATRIBUTE_HPP_

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>

namespace sbx::scripting::managed {

class attribute {

  friend class type;
  friend class method_info;
  friend class field_info;
  friend class property_info;

public:

  auto get_type() -> type&;

  template<typename Return>
  auto get_field_value(std::string_view field_name) -> Return {
    auto result = Return{};

    _get_field_value_internal(field_name, &result);

    return result;
  }

private:

  auto _get_field_value_internal(std::string_view field_name, void* value) const -> void;

  handle _handle = -1;
  type* _type = nullptr;

}; // class attribute 

template<>
auto attribute::get_field_value<std::string>(std::string_view field_name) -> std::string;

template<>
auto attribute::get_field_value<bool>(std::string_view field_name) -> bool;

}; // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_ATRIBUTE_HPP_