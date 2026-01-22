// SPDX-License-Identifier: MIT
#ifndef LIBSBX_DEVICES_INPUT_ACTION_HPP_
#define LIBSBX_DEVICES_INPUT_ACTION_HPP_

#include <cinttypes>

#include <GLFW/glfw3.h>

namespace sbx::devices {

enum class input_action : std::int32_t {
  release = GLFW_RELEASE,
  press = GLFW_PRESS,
  repeat = GLFW_REPEAT,
}; // enum class input_action

} // namespace sbx::devices

#endif // LIBSBX_DEVICES_INPUT_ACTION_HPP_
