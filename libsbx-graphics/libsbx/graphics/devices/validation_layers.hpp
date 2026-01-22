// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_VALIDATION_LAYERS_HPP_
#define LIBSBX_GRAPHICS_DEVICES_VALIDATION_LAYERS_HPP_

#include <vector>

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/target.hpp>

namespace sbx::graphics {

struct validation_layers {

  static auto instance() -> std::vector<const char*>;

}; // struct validation_layers

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_VALIDATION_LAYERS_HPP_
