// SPDX-License-Identifier: MIT
#ifndef LIBSBX_DEVICES_INPUT_MOD_HPP_
#define LIBSBX_DEVICES_INPUT_MOD_HPP_

#include <cinttypes>

#include <GLFW/glfw3.h>

#include <libsbx/utility/bitmask.hpp>

namespace sbx::devices {

enum class input_mod : std::int32_t {
  shift = GLFW_MOD_SHIFT,
  control = GLFW_MOD_CONTROL,
  alt = GLFW_MOD_ALT,
  super = GLFW_MOD_SUPER,
  caps_lock = GLFW_MOD_CAPS_LOCK,
  num_lock = GLFW_MOD_NUM_LOCK,
}; // enum class input_mod

} // namespace sbx::devices

template<>
struct sbx::utility::enable_bitmask_operators<sbx::devices::input_mod> : std::true_type { };

#endif // LIBSBX_DEVICES_INPUT_MOD_HPP_
