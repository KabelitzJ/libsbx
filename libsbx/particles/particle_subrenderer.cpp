// SPDX-License-Identifier: MIT
#include <libsbx/particles/particle_subrenderer.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::particles {

particle_subrenderer::particle_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path)
: graphics::subrenderer{},
  _pipeline{path, attachments, definition},
  _push_handler{_pipeline} { }

auto particle_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("particle_subrenderer::render");
  SBX_PROFILE_SCOPE("particle_subrenderer::render");

  auto timer = graphics::scoped_gpu_timer{command_buffer, "particle_subrenderer"};

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto particle_task = renderer.task<particles::particle_task>();

  if (!particle_task) {
    utility::logger<"particles">::warn("The particle_subrenderer requires 'sbx::particlesparticle_task' to run");

    return;
  }

  std::erase_if(_descriptor_data, [&](const auto& entry) {
    const auto& [node, handler] = entry;

    return !graph.is_valid(node) || !graph.has_component<particle_emitter>(node);
  });

  auto emitter_query = graph.query<const particle_emitter>();

  for (auto&& [node, emitter] : emitter_query.each()) {
    utility::assert_that(emitter.images.size() > 0u, "Invalid particle_emitter with 0 images");
    utility::assert_that(emitter.images, [](const auto& handle) -> bool { return handle.is_valid(); }, "Invalid image handle in particle_emitter");

    auto particle_buffer = particle_task->particle_buffer(node);
    auto alive_list_buffer = particle_task->alive_list_buffer(node);
    auto indirect_buffer = particle_task->indirect_buffer(node);

    utility::assert_that(particle_buffer.is_valid() && alive_list_buffer.is_valid() && indirect_buffer.is_valid(), "Invalid buffer handles");

    auto& particles = graphics_module.get_resource<graphics::storage_buffer>(particle_buffer);
    auto& alive_list = graphics_module.get_resource<graphics::storage_buffer>(alive_list_buffer);
    auto& indirect = graphics_module.get_resource<graphics::storage_buffer>(indirect_buffer);

    auto entry = _descriptor_data.find(node);

    if (entry == _descriptor_data.end()) {
      auto [inserted, success] = _descriptor_data.emplace(node, graphics::descriptor_handler{_pipeline, 0u});

      entry = inserted;
    }

    auto& [descriptor_handler, images, sampler] = entry->second;
    images.clear();

    _pipeline.bind(command_buffer);

    descriptor_handler.push("scene", environment.uniform_handler());
    descriptor_handler.push("particles", particles);
    descriptor_handler.push("alive_list", alive_list);

    for (const auto& handle : emitter.images) {
      images.push_back(handle);
    }

    descriptor_handler.push("images", images);
    descriptor_handler.push("sampler", sampler);

    _push_handler.push("end_color", emitter.end_color);
    _push_handler.push("end_size_scale", emitter.end_size_scale);

    descriptor_handler.update(_pipeline);
    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw_indirect(indirect, 0u, 1u);
  }
}

} // namespace sbx::particles
