// SPDX-License-Identifier: MIT
#include <libsbx/animations/skinned_mesh_shadow_subrenderer.hpp>

namespace sbx::animations {

skinned_mesh_shadow_subrenderer::skinned_mesh_shadow_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const std::uint32_t cascade, const graphics::storage_buffer_handle skinned_vertex_buffer)
: graphics::subrenderer{},
  _attachments{attachments},
  _base_pipeline{base_pipeline},
  _cascade{cascade},
  _skinned_vertex_buffer{skinned_vertex_buffer} {

}

skinned_mesh_shadow_subrenderer::~skinned_mesh_shadow_subrenderer() {
  _pipeline_cache.clear();
}

auto skinned_mesh_shadow_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  EASY_FUNCTION();

  SBX_PROFILE_SCOPE("skinned_mesh_shadow_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

  auto& draw_list = renderer.draw_list<skinned_mesh_material_draw_list>("skinned_mesh_material");

  const auto& ranges = draw_list.ranges(skinned_mesh_material_draw_list::bucket::shadow);

  for (auto& [key, data] : ranges) {
    auto& pipeline_data = _get_or_create_pipeline(key);
    auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

    pipeline.bind(command_buffer);

    pipeline_data.scene_descriptor_handler.push("scene", scene.uniform_handler());

    if (!pipeline_data.scene_descriptor_handler.update(pipeline)) {
      return;
    }

    pipeline_data.scene_descriptor_handler.bind_descriptors(command_buffer);

    pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(models::static_mesh_material_draw_list::transform_data_buffer_name).address());
    pipeline_data.push_handler.push("instance_data_buffer", graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer).address());
    pipeline_data.push_handler.push("vertex_buffer", graphics_module.get_resource<graphics::storage_buffer>(_skinned_vertex_buffer).address());
    pipeline_data.push_handler.push("cascade", _cascade);

    auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

    for (const auto& draw_range : data.ranges) {
      auto& mesh = assets_module.get_asset<animations::mesh>(draw_range.mesh_id);

      mesh.bind(command_buffer);

      pipeline_data.push_handler.bind(command_buffer);

      command_buffer.draw_indexed_indirect(draw_commands_buffer, draw_range.range.offset, draw_range.range.count);
    }
  }
}

skinned_mesh_shadow_subrenderer::pipeline_data::pipeline_data(const graphics::graphics_pipeline_handle& handle)
: pipeline{handle},
  push_handler{pipeline},
  scene_descriptor_handler{pipeline, 0u} { }

auto skinned_mesh_shadow_subrenderer::_get_or_create_pipeline(const models::material_key& key) -> pipeline_data& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (auto it = _pipeline_cache.find(key); it != _pipeline_cache.end()) {
    return it->second;
  }

  auto definition = pipeline_definition;
  definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::front;

  auto& compiler = graphics_module.compiler();

  const auto request = graphics::compiler::compile_request{
    .path = _base_pipeline,
    .per_stage = {
      {SLANG_STAGE_VERTEX, {.entry_point = "main"}},
      { SLANG_STAGE_FRAGMENT, {.entry_point = "main"}}
    }
  };

  const auto result = compiler.compile(request);

  auto compiled = graphics::graphics_pipeline::compiled_shaders{_base_pipeline.filename().string(), result.code};
  auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(key, handle);

  return entry->second;
}

} // namespace sbx::animations
