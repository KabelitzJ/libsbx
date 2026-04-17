// SPDX-License-Identifier: MIT
#ifndef DEMO_RENDERER_HPP_
#define DEMO_RENDERER_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

namespace demo {

class renderer : public sbx::graphics::renderer {

  using base = sbx::graphics::renderer;

public:

  renderer();

  ~renderer() override = default;

private:

  sbx::math::color _clear_color;

}; // class renderer

} // namespace demo

#endif // DEMO_RENDERER_HPP_
