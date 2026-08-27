// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_PIPELINE_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_PIPELINE_CACHE_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

namespace sbx::graphics {

/**
 * @brief The complete set of non-dynamic state that defines a graphics pipeline's identity.
 *
 * Everything that affects `vkCreateGraphicsPipelines` and is not dynamic state (viewport/scissor)
 * and not constant across the engine (the shared bindless layout, absence of vertex input). Two
 * pipelines are the same iff their pipeline_state compares equal — this is the cache key.
 */
struct pipeline_state {
  graphics::shader::id_type shader{0u};
  std::vector<graphics::format> color_formats{};
  graphics::format depth_format{format::undefined};
  graphics::primitive_topology topology{primitive_topology::triangle_list};
  bool primitive_restart{false};
  graphics::polygon_mode polygon_mode{polygon_mode::fill};
  graphics::cull_mode cull_mode{cull_mode::none};
  graphics::front_face front_face{front_face::counter_clockwise};
  bool depth_bias_enable{false};
  std::float_t depth_bias_constant{0.0f};
  std::float_t depth_bias_slope{0.0f};
  std::float_t depth_bias_clamp{0.0f};
  bool depth_test{false};
  bool depth_write{false};
  graphics::compare_operation depth_compare{compare_operation::less_or_equal};
  graphics::samples samples{samples::count_1};
  std::vector<graphics::blend_attachment> color_blend_attachments{};
  std::vector<graphics::specialization_constant> specialization_constants{};

  auto operator==(const pipeline_state&) const -> bool = default;

}; // struct pipeline_state

struct pipeline_state_hash {
  auto operator()(const pipeline_state& state) const noexcept -> std::size_t;
}; // struct pipeline_state_hash

/**
 * @brief Owns every graphics pipeline for the run, deduplicated by @ref pipeline_state.
 *
 * Not thread-safe — pipeline creation is a render-thread operation, like the resource pool. The
 * renderer holds the returned reference (non-owning); the cache and its pipelines live and die with
 * the graphics module, whose destructor waits for the device to idle first.
 */
class pipeline_cache : public utility::noncopyable {

public:

  pipeline_cache() = default;

  ~pipeline_cache() = default;

  [[nodiscard]] auto get(const graphics_pipeline::create_info& create_info) -> memory::observer_ptr<graphics_pipeline>;

private:

  std::unordered_map<pipeline_state, std::unique_ptr<graphics_pipeline>, pipeline_state_hash> _pipelines{};

}; // class pipeline_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_PIPELINE_CACHE_HPP_
