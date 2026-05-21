// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_VERTEX3D_HPP_
#define LIBSBX_MODELS_VERTEX3D_HPP_

#include <utility>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

namespace sbx::models {

struct alignas(alignof(std::float_t)) vertex3d {
  math::vector3 position;
  math::vector3 normal;
  math::vector2 uv0;
  math::vector2 uv1;
  math::vector4 tangent;
}; // struct vertex

static_assert(sizeof(vertex3d) == 56, "Incorrect sbx::models::vertex3d size");
static_assert(alignof(vertex3d) == 4, "Incorrect sbx::models::vertex3d alignment");

constexpr auto operator==(const vertex3d& lhs, const vertex3d& rhs) noexcept -> bool {
  return lhs.position == rhs.position && lhs.normal == rhs.normal && lhs.tangent == rhs.tangent && lhs.uv0 == rhs.uv0 && lhs.uv1 == rhs.uv1;
}

} // namespace sbx::models

template<>
struct sbx::graphics::vertex_input<sbx::models::vertex3d> {
  static auto description() -> sbx::graphics::vertex_input_description {
    auto result = vertex_input_description{};

    result.binding_descriptions.push_back(VkVertexInputBindingDescription{
      .binding = 0,
      .stride = sizeof(sbx::models::vertex3d),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(sbx::models::vertex3d, position)
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(sbx::models::vertex3d, normal)
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 2,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = offsetof(sbx::models::vertex3d, tangent)
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 3,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(sbx::models::vertex3d, uv0)
    });

    result.attribute_descriptions.push_back(VkVertexInputAttributeDescription{
      .location = 4,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(sbx::models::vertex3d, uv1)
    });

    return result;
  }
}; // struct sbx::graphics::vertex_input

template<>
struct std::hash<sbx::models::vertex3d> {
  auto operator()(const sbx::models::vertex3d& vertex) const noexcept -> std::size_t {
    auto hash = std::size_t{0};
    sbx::utility::hash_combine(hash, vertex.position, vertex.normal, vertex.tangent, vertex.uv0, vertex.uv1);
    return hash;
  }
}; // struct std::hash<vertex3d>

#endif // LIBSBX_MODELS_VERTEX3D_HPP_
