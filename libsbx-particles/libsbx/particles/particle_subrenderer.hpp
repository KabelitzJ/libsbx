// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PARTICLES_PARTICLE_SUBRENDERER_HPP_
#define LIBSBX_PARTICLES_PARTICLE_SUBRENDERER_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/subrenderer.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/particles/particle_emitter.hpp>
#include <libsbx/particles/particle_task.hpp>

namespace sbx::particles {

class particle_subrenderer final : public graphics::subrenderer {

  inline static const auto definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_only,
    .uses_transparency = true,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::none,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

public:

  particle_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, memory::observer_ptr<const particle_task> particle_task);

  ~particle_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override;

private:

  struct render_params {
    math::color end_color;
    float end_size_scale;
    std::uint32_t image_index;
    std::uint32_t _pad0;
    std::uint32_t _pad1;
  }; // struct render_params

  memory::observer_ptr<const particle_task> _particle_task;

  graphics::graphics_pipeline _pipeline;
  graphics::push_handler _push_handler;

  std::unordered_map<scenes::node, graphics::descriptor_handler> _descriptor_handlers;

}; // class particle_subrenderer

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLE_SUBRENDERER_HPP_