// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_IMAGE_HPP_
#define LIBSBX_UI_IMAGE_HPP_

#include <libsbx/graphics/images/image.hpp>

#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class image : public element {

public:

  graphics::image2d_handle source{};

  image() = default;

  ~image() override = default;

protected:

  auto submit(sprites::sprites_module& sprites, const math::vector2& screen_size) -> void override {
    const auto albedo_index = sprites.register_image(source);

    const auto& r = computed_rect();

    const auto pivot_x = r.x + r.width * pivot.x();
    const auto pivot_y = r.y + r.height * pivot.y();

    submit_quad(sprites, screen_size, {pivot_x, pivot_y}, {r.width, r.height}, pivot, color, albedo_index, sort_order, {0.0f, 0.0f}, {1.0f, 1.0f}, 0u);
  }

}; // class image

} // namespace sbx::ui

#endif // LIBSBX_UI_IMAGE_HPP_
