// SPDX-License-Identifier: MIT
#include <libsbx/particles/particle_subrenderer.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::particles {

particle_subrenderer::particle_subrenderer(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, memory::observer_ptr<const particle_task> particle_task)
: graphics::subrenderer{},
  _pipeline{path, attachments, definition},
  _push_handler{_pipeline},
  _particle_task{particle_task} { }

auto particle_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  EASY_FUNCTION();

  SBX_PROFILE_SCOPE("particle_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  std::erase_if(_descriptor_data, [&](const auto& entry) {
    const auto& [node, handler] = entry;

    return !scene.is_valid(node) || !scene.has_component<particle_emitter>(node);
  });

  auto emitter_query = scene.query<const particle_emitter>();

  for (auto&& [node, emitter] : emitter_query.each()) {
    utility::assert_that(emitter.images.size() > 0u, "Invalid particle_emitter with 0 images");
    utility::assert_that(emitter.images, [](const auto& handle) -> bool { return handle.is_valid(); }, "Invalid image handle in particle_emitter");

    auto particle_buffer = _particle_task->particle_buffer(node);
    auto alive_list_buffer = _particle_task->alive_list_buffer(node);
    auto indirect_buffer = _particle_task->indirect_buffer(node);

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

    descriptor_handler.push("scene", scene.uniform_handler());
    descriptor_handler.push("particles", particles);
    descriptor_handler.push("alive_list", alive_list);

    for (const auto& handle : emitter.images) {
      images.push_back(handle);
    }

    descriptor_handler.push("images", images);
    descriptor_handler.push("sampler", sampler);

    auto params = render_params{
      .end_color = emitter.end_color,
      .end_size_scale = emitter.end_size_scale,
      ._pad0 = 0,
      ._pad1 = 0,
      ._pad2 = 0
    };

    _push_handler.push(params);

    descriptor_handler.update(_pipeline);
    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw_indirect(indirect, 0u, 1u);
  }
}

} // namespace sbx::particles
