// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::render {

/**
 * @brief Owns the render stages and drives the frame loop.
 */
class render_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<assets::assets_module, graphics::graphics_module, scenes::scenes_module>;

  render_module();

  ~render_module();

  auto render() -> void;

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
