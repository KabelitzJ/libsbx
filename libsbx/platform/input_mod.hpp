// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_INPUT_MOD_HPP_
#define LIBSBX_PLATFORM_INPUT_MOD_HPP_

#include <cinttypes>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/bit.hpp>

namespace sbx::platform {

enum class [[=reflection::bit_field]] input_mod : std::int32_t {
  shift = utility::bit_v<0>,
  control = utility::bit_v<1>,
  alt = utility::bit_v<2>,
  super = utility::bit_v<3>,
  caps_lock = utility::bit_v<4>,
  num_lock = utility::bit_v<5>,
}; // enum class input_mod

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_INPUT_MOD_HPP_
