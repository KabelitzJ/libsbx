// SPDX-License-Identifier: MIT
#ifndef LIBSBX_REFLECTION_ENUMERATOR_HPP_
#define LIBSBX_REFLECTION_ENUMERATOR_HPP_

#include <string_view>

namespace sbx::reflection {

template<typename Enum>
struct enumerator {

  using enum_type = Enum;

  std::string_view name;
  Enum value;

}; // struct enumerator

template<typename Enum>
enumerator(std::string_view, Enum) -> enumerator<Enum>;

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ENUMERATOR_HPP_
