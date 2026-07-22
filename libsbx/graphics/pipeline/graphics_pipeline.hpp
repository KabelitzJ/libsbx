// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>

namespace sbx::graphics {

/**
 * @brief A graphics pipeline for dynamic rendering, built against the shared bindless pipeline
 * layout. Viewport and scissor are dynamic, so a resize needs no rebuild. No vertex input state —
 * geometry comes from SV_VertexID or (later) vertex pulling via buffer device address.
 */
class graphics_pipeline : public utility::noncopyable {

public:

  using handle_type = VkPipeline;

  struct create_info {
    const graphics::shader* shader{};
    std::vector<graphics::format> color_formats{};
    graphics::format depth_format{format::undefined};
    graphics::primitive_topology topology{primitive_topology::triangle_list};
    graphics::polygon_mode polygon_mode{polygon_mode::fill};
    graphics::cull_mode cull_mode{cull_mode::none};
    graphics::front_face front_face{front_face::counter_clockwise};
    bool depth_test{false};
    bool depth_write{false};
    std::string name{"Pipeline"};
  }; // struct create_info

  explicit graphics_pipeline(const create_info& create_info);

  ~graphics_pipeline();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto bind_point() const noexcept -> VkPipelineBindPoint {
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

private:

  handle_type _handle{};

}; // class pipeline

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
