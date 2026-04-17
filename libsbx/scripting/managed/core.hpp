// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_CORE_HPP_
#define LIBSBX_SCRIPTING_MANAGED_CORE_HPP_

#include <cstdint>

#include <libsbx/scripting/managed/platform.hpp>

namespace sbx::scripting::managed {

using bool32 = std::uint32_t;

enum class type_accessibility : std::uint8_t {
  public_access,
  private_access,
  protected_access,
  internal_access,
  protected_public_access,
  private_protected_access
}; // enum class type_accessibility

using type_id = std::int32_t;
using handle = std::int32_t;

struct internal_call {
  const char_type* name;
  void* native_function_pointer;
}; // struct internal_call

} // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_CORE_HPP_