// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_INPUT_MOD_HPP_
#define LIBSBX_PLATFORM_INPUT_MOD_HPP_

#include <cinttypes>

#include <libsbx/utility/bitmask.hpp>

namespace sbx::platform {

// Values match the glfw modifier bits; verified by static_asserts in window.cpp.
enum class input_mod : std::int32_t {
  shift = 0x0001,
  control = 0x0002,
  alt = 0x0004,
  super = 0x0008,
  caps_lock = 0x0010,
  num_lock = 0x0020,
}; // enum class input_mod

} // namespace sbx::platform

template<>
struct sbx::utility::enable_bitmask_operators<sbx::platform::input_mod> : std::true_type { };

#endif // LIBSBX_PLATFORM_INPUT_MOD_HPP_
