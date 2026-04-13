// SPDX-License-Identifier: MIT
#ifndef LIBSBX_REFLECTION_MEMBER_HPP_
#define LIBSBX_REFLECTION_MEMBER_HPP_

#include <string_view>

namespace sbx::reflection {

template<typename Owner, typename Type>
struct member {

  using owner_type = Owner;
  using value_type = Type;

  std::string_view name;
  value_type owner_type::* pointer;

}; // struct member

template<typename Owner, typename Type>
member(std::string_view, Type Owner::*) -> member<Owner, Type>;

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_MEMBER_HPP_