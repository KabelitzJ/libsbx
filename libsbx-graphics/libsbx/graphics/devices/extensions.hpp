// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_EXTENSIONS_HPP_
#define LIBSBX_GRAPHICS_DEVICES_EXTENSIONS_HPP_

#include <vector>

#include <fmt/format.h>

#include <vulkan/vulkan.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/target.hpp>

#include <libsbx/devices/devices_module.hpp>

namespace sbx::graphics {

struct extensions {

  static auto device() -> std::vector<const char*>;

  static auto instance() -> std::vector<const char*>;

}; // struct extensions

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_EXTENSIONS_HPP_
