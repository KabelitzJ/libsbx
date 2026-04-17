// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_FIELD_INFO_HPP_
#define LIBSBX_SCRIPTING_MANAGED_FIELD_INFO_HPP_

#include <vector>

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/attribute.hpp>

namespace sbx::scripting::managed {

class field_info {

  friend class Type;

public:

  auto get_name() const -> string;

  auto get_type() -> type&;

  auto get_accessibility() const -> type_accessibility;

  auto get_attributes() const -> std::vector<attribute>;

private:

  handle _handle = -1;
  type* _type = nullptr;

}; // class field_info

} // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_FIELD_INFO_HPP_