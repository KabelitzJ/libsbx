// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_INPUT_ACTION_HPP_
#define LIBSBX_PLATFORM_INPUT_ACTION_HPP_

#include <cinttypes>

namespace sbx::platform {

// Values match the glfw actions; verified by static_asserts in window.cpp.
enum class input_action : std::int32_t {
  release = 0,
  press = 1,
  repeat = 2,
}; // enum class input_action

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_INPUT_ACTION_HPP_
