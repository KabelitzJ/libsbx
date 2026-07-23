// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_

#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>

namespace sbx::graphics {

/**
 * @brief Per-attachment color blend state. Defaults to blending disabled with a full RGBA write mask.
 */
struct blend_attachment {
  bool enable{false};
  graphics::blend_factor source_color{blend_factor::one};
  graphics::blend_factor destination_color{blend_factor::zero};
  graphics::blend_operation color_operation{blend_operation::add};
  graphics::blend_factor source_alpha{blend_factor::one};
  graphics::blend_factor destination_alpha{blend_factor::zero};
  graphics::blend_operation alpha_operation{blend_operation::add};
  color_component color_write_mask{color_component::r | color_component::g | color_component::b | color_component::a};

  auto operator==(const blend_attachment&) const -> bool = default;
}; // struct blend_attachment

/**
 * @brief A graphics pipeline for dynamic rendering, built against the shared bindless pipeline
 * layout. Viewport and scissor are dynamic, so a resize needs no rebuild. No vertex input state —
 * geometry comes from SV_VertexID or vertex pulling via buffer device address.
 *
 * Owned by @ref pipeline_cache; construct via the cache, not directly.
 */
class graphics_pipeline : public utility::noncopyable {

public:

  using handle_type = VkPipeline;

  struct create_info {
    memory::observer_ptr<const graphics::shader> shader{};
    std::vector<graphics::format> color_formats{};
    graphics::format depth_format{format::undefined};
    graphics::primitive_topology topology{primitive_topology::triangle_list};
    bool primitive_restart{false};
    graphics::polygon_mode polygon_mode{polygon_mode::fill};
    graphics::cull_mode cull_mode{cull_mode::none};
    graphics::front_face front_face{front_face::counter_clockwise};
    std::optional<graphics::depth_bias> depth_bias{};
    bool depth_test{false};
    bool depth_write{false};
    graphics::compare_operation depth_compare{compare_operation::less_or_equal};
    graphics::samples samples{samples::count_1};
    std::vector<graphics::blend_attachment> color_blend_attachments{};
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

  [[nodiscard]] auto bind_point() const noexcept -> pipeline_bind_point {
    return pipeline_bind_point::graphics;
  }

private:

  handle_type _handle{};

}; // class graphics_pipeline

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
