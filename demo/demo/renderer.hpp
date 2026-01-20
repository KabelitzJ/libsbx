#ifndef DEMO_RENDERER_HPP_
#define DEMO_RENDERER_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <demo/data.hpp>
#include <demo/terrain_subrenderer.hpp>

namespace demo {

class renderer : public sbx::graphics::renderer {

  using base = sbx::graphics::renderer;

public:

  renderer();

  ~renderer() override = default;

  auto update_dual_grid_data(const dual_grid<grid_data>& grid) -> void {
    _terrain_subrenderer->update_dual_grid_data(grid);
  }

private:

  sbx::math::color _clear_color;
  sbx::memory::observer_ptr<terrain_subrenderer> _terrain_subrenderer;

}; // class renderer

} // namespace demo

#endif // DEMO_RENDERER_HPP_
