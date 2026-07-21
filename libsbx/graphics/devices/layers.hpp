// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_LAYERS_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LAYERS_HPP_

#include <vector>

namespace sbx::graphics {

struct layers {

  static auto instance() -> std::vector<const char*>;

}; // struct layers

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LAYERS_HPP_
