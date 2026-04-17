// SPDX-License-Identifier: MIT
#include <libsbx/ui/image.hpp>

namespace sbx::ui {

auto image::submit(const math::vector2& screen_size) -> void {
  auto& sprites_module = core::engine::get_module<sprites::sprites_module>();


  const auto albedo_index = std::visit([&](auto&& src) {
    return sprites_module.register_image(src);
  }, source);

  const auto& r = computed_rectangle();

  const auto pivot_x = r.x + r.width * pivot.x();
  const auto pivot_y = r.y + r.height * pivot.y();

  submit_quad(screen_size, {pivot_x, pivot_y}, {r.width, r.height}, pivot, color, albedo_index, sort_order, {0.0f, 1.0f}, {1.0f, 0.0f}, 0u);
}

} // namespace sbx::ui