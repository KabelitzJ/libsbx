// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_PROPERTY_INFO_HPP_
#define LIBSBX_SCRIPTING_MANAGED_PROPERTY_INFO_HPP_

#include <vector>

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/attribute.hpp>

namespace sbx::scripting::managed {

class property_info {

  friend class type;

public:

  auto get_name() const -> string;
  
  auto get_type() -> type&;

  auto get_attributes() const -> std::vector<attribute>;

private:

  handle _handle = -1;
  type* _type = nullptr;

}; // class property_info

} // sbx

#endif // LIBSBX_SCRIPTING_MANAGED_PROPERTY_INFO_HPP_