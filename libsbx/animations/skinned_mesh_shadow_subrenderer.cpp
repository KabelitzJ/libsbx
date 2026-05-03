// SPDX-License-Identifier: MIT
#include <libsbx/animations/skinned_mesh_shadow_subrenderer.hpp>

namespace sbx::animations {

skinned_mesh_shadow_subrenderer::skinned_mesh_shadow_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline)
: graphics::subrenderer{},
  _attachments{attachments},
  _base_pipeline{base_pipeline} { }

skinned_mesh_shadow_subrenderer::~skinned_mesh_shadow_subrenderer() {
  _pipeline_cache.clear();
}

auto skinned_mesh_shadow_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("skinned_mesh_shadow_subrenderer::render");
  SBX_PROFILE_GPU_SCOPE(command_buffer, "skinned_mesh_shadow_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& graph = scene.graph();
  auto& environment = scene.environment();

  auto& draw_list = renderer.draw_list<skinned_mesh_material_draw_list>();

  const auto& ranges = draw_list.ranges(skinned_mesh_material_draw_list::bucket::shadow);

  auto skinning_task = renderer.task<animations::skinning_task>();

  if (!skinning_task) {
    utility::logger<"animations">::error("Skinning task is not available. Cannot render skinned meshes.");
    return;
  }

  for (auto& [key, data] : ranges) {
    auto& pipeline_data = _get_or_create_pipeline(key);
    auto& descriptor_data = _get_or_create_descriptor_data(pipeline_data.pipeline);

    auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

    pipeline.bind(command_buffer);

    descriptor_data.scene_descriptor_handler.push("scene", environment.uniform_handler());
    descriptor_data.sampler_descriptor_handler.push("samplers", draw_list.samplers());
    descriptor_data.image_descriptor_handler.push("images", draw_list.images());

    auto update_successful = true;

    update_successful &= descriptor_data.scene_descriptor_handler.update(pipeline);
    update_successful &= descriptor_data.sampler_descriptor_handler.update(pipeline);
    update_successful &= descriptor_data.image_descriptor_handler.update(pipeline);

    if (!update_successful) {
      return;
    }

    descriptor_data.scene_descriptor_handler.bind_descriptors(command_buffer);
    descriptor_data.sampler_descriptor_handler.bind_descriptors(command_buffer);
    descriptor_data.image_descriptor_handler.bind_descriptors(command_buffer);

    pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::transform_data_buffer_name).address());
    pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::material_data_buffer_name).address());
    pipeline_data.push_handler.push("instance_data_buffer", graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer).address());
    pipeline_data.push_handler.push("vertex_buffer", graphics_module.get_resource<graphics::storage_buffer>(skinning_task->vertex_buffer_handle()).address());

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
  push_handler{pipeline} { }

skinned_mesh_shadow_subrenderer::descriptor_data::descriptor_data(const graphics::graphics_pipeline_handle& handle)
: scene_descriptor_handler{handle, 0u},
  sampler_descriptor_handler{handle, 1u},
  image_descriptor_handler{handle, 2u} { }

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
      {SLANG_STAGE_VERTEX, { .entry_point = "main" }},
      { SLANG_STAGE_FRAGMENT, { .entry_point = _entry_point.at(key.alpha) }}
    }
  };

  const auto result = compiler.compile(request);

  auto compiled = graphics::graphics_pipeline::compiled_shaders{_base_pipeline.filename().string(), result.code};
  auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(key, handle);

  return entry->second;
}

auto skinned_mesh_shadow_subrenderer::_get_or_create_descriptor_data(const graphics::graphics_pipeline_handle& handle) -> descriptor_data& {
  if (auto it = _descriptor_cache.find(handle); it != _descriptor_cache.end()) {
    return it->second;
  }

  auto [entry, inserted] = _descriptor_cache.emplace(handle, descriptor_data{handle});

  return entry->second;
}

} // namespace sbx::animations
