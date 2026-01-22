// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_PIPELINE_VERTEX_INPUT_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_VERTEX_INPUT_HPP_

#include <vector>

#include <vulkan/vulkan.hpp>

#include <libsbx/containers/static_vector.hpp>

namespace sbx::graphics {

struct vertex_input_description {
  containers::static_vector<VkVertexInputBindingDescription, 2u> binding_descriptions;
  containers::static_vector<VkVertexInputAttributeDescription, 5u> attribute_descriptions;
}; // struct vertex_input

template<typename Vertex>
struct vertex_input {
  static auto description() -> vertex_input_description;
}; // struct vertex_input

template<typename Type>
concept vertex = requires {
  { vertex_input<Type>::description() } -> std::same_as<vertex_input_description>;
};

struct empty_vertex { };

constexpr auto operator==([[maybe_unused]] const empty_vertex& lhs, [[maybe_unused]] const empty_vertex& rhs) noexcept -> bool {
  return true;
}

} // namespace sbx::graphics

template<>
struct sbx::graphics::vertex_input<sbx::graphics::empty_vertex> {
  static auto description() -> sbx::graphics::vertex_input_description {
    return sbx::graphics::vertex_input_description{};
  }
}; // struct sbx::graphics::vertex_input<sbx::models::vertex3d>

template<>
struct std::hash<sbx::graphics::empty_vertex> {
  auto operator()([[maybe_unused]] const sbx::graphics::empty_vertex& vertex) const noexcept -> std::size_t {
    return 0u;
  }
}; // struct std::hash<vertex3d>

#endif // LIBSBX_GRAPHICS_PIPELINE_VERTEX_INPUT_HPP_
