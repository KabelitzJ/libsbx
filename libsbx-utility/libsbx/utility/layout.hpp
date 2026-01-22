// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_LAYOUT_HPP_
#define LIBSBX_UTILITY_LAYOUT_HPP_

#include <utility>

namespace sbx::utility {

template<typename Type, std::size_t Size, std::size_t Alignment>
struct layout_requirements {
  inline static constexpr auto value = sizeof(Type) == Size && alignof(Type) == Alignment;
}; // struct layout_requirements

template<typename Type, std::size_t Size, std::size_t Alignment>
inline constexpr auto layout_requirements_v = layout_requirements<Type, Size, Alignment>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_LAYOUT_HPP_
