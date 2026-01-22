// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_VERTEX2D_HPP_
#define LIBSBX_UI_VERTEX2D_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

namespace sbx::ui {

struct vertex2d {
  math::vector2 position;
  math::vector2 uv;
}; // struct vertex

constexpr auto operator==(const vertex2d& lhs, const vertex2d& rhs) noexcept -> bool {
  return lhs.position == rhs.position && lhs.uv == rhs.uv;
}

} // namespace sbx::ui

template<>
struct sbx::graphics::vertex_input<sbx::ui::vertex2d> {
  static auto description() -> sbx::graphics::vertex_input_description {
    auto result = sbx::graphics::vertex_input_description{};

    result.binding_descriptions.push_back(VkVertexInputBindingDescription{
      .binding = 0,
      .stride = sizeof(sbx::ui::vertex2d),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(sbx::ui::vertex2d, position)
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(sbx::ui::vertex2d, uv)
    });

    return result;
  }
}; // struct sbx::graphics::vertex_input

#endif // LIBSBX_UI_VERTEX2D_HPP_
