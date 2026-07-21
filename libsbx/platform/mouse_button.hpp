// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_MOUSE_BUTTON_HPP_
#define LIBSBX_PLATFORM_MOUSE_BUTTON_HPP_

#include <cinttypes>

namespace sbx::platform {

// Values match the glfw buttons; verified by static_asserts in window.cpp.
enum class mouse_button : std::int32_t {
  one = 0,
  two = 1,
  three = 2,
  four = 3,
  five = 4,
  six = 5,
  seven = 6,
  eight = 7,
  left = one,
  right = two,
  middle = three,
}; // enum class mouse_button

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_MOUSE_BUTTON_HPP_
