// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_FOURCC_HPP_
#define LIBSBX_UTILITY_FOURCC_HPP_

#include <libsbx/utility/string_literal.hpp>

namespace sbx::utility {

template<utility::string_literal Name>
requires (Name.size() == 4u)
struct fourcc {
  static constexpr auto value = std::uint32_t{
    (static_cast<std::uint32_t>(Name[0]) << 0u) |
    (static_cast<std::uint32_t>(Name[1]) << 8u) |
    (static_cast<std::uint32_t>(Name[2]) << 16u) |
    (static_cast<std::uint32_t>(Name[3]) << 24u)
  };
}; // struct fourcc

template<utility::string_literal Name>
requires (Name.size() == 4u)
constexpr auto fourcc_v = fourcc<Name>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_FOURCC_HPP_
