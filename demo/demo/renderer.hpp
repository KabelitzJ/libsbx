// SPDX-License-Identifier: MIT
#ifndef DEMO_RENDERER_HPP_
#define DEMO_RENDERER_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain.hpp>
#include <demo/terrain_subrenderer.hpp>

namespace demo {

class renderer : public sbx::graphics::renderer {

  using base = sbx::graphics::renderer;

public:

  renderer(bool is_editor, const terrain& terrain);

  ~renderer() override = default;

  auto terrain_renderer() -> terrain_subrenderer& {
    return *_terrain_subrenderer;
  }

private:

  sbx::math::color _clear_color;
  sbx::memory::observer_ptr<terrain_subrenderer> _terrain_subrenderer;

}; // class renderer

} // namespace demo

#endif // DEMO_RENDERER_HPP_
