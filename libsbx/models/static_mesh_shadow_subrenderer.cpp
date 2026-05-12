// SPDX-License-Identifier: MIT
#include <libsbx/models/static_mesh_shadow_subrenderer.hpp>

namespace sbx::models {

static_mesh_shadow_subrenderer::static_mesh_shadow_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline)
: graphics::subrenderer{},
  _attachments{attachments},
  _base_pipeline{base_pipeline} { }

static_mesh_shadow_subrenderer::~static_mesh_shadow_subrenderer() {
  _pipeline_cache.clear();
}

auto static_mesh_shadow_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("static_mesh_shadow_subrenderer::render");
  SBX_PROFILE_GPU_SCOPE(command_buffer, "static_mesh_shadow_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();

  auto& draw_list = renderer.draw_list<models::static_mesh_material_draw_list>();

  auto frustum_culling_task = renderer.task<models::frustum_culling_task>();

  const auto& ranges = draw_list.ranges(models::static_mesh_material_draw_list::bucket::shadow);

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

    auto culled = frustum_culling_task ? frustum_culling_task->culled(models::bucket::shadow, key) : std::nullopt;

    auto& instance_data_buffer = graphics_module.get_resource<graphics::storage_buffer>(culled ? culled->instances_buffer : data.instance_data_buffer);
    auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(culled ? culled->commands_buffer : data.draw_commands_buffer);

    pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(static_mesh_material_draw_list::transform_data_buffer_name).address());
    pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(static_mesh_material_draw_list::material_data_buffer_name).address());
    pipeline_data.push_handler.push("instance_data_buffer", instance_data_buffer.address());

    for (const auto& range_ref : data.ranges) {
      auto& mesh = assets_module.get_asset<models::mesh>(range_ref.mesh_id);

      mesh.bind(command_buffer);

      pipeline_data.push_handler.push("vertex_buffer", mesh.address());

      pipeline_data.push_handler.bind(command_buffer);

      command_buffer.draw_indexed_indirect(draw_commands_buffer, range_ref.range.offset, range_ref.range.count);
    }
  }
}

static_mesh_shadow_subrenderer::pipeline_data::pipeline_data(const graphics::graphics_pipeline_handle& handle)
: pipeline{handle},
  push_handler{pipeline} { }

static_mesh_shadow_subrenderer::descriptor_data::descriptor_data(const graphics::graphics_pipeline_handle& handle)
: scene_descriptor_handler{handle, 0u},
  sampler_descriptor_handler{handle, 1u},
  image_descriptor_handler{handle, 2u} { }

auto static_mesh_shadow_subrenderer::_get_or_create_pipeline(const models::material_key& key) -> pipeline_data& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto lookup_key = key;
  lookup_key.stream_mask = 0u;

  if (auto it = _pipeline_cache.find(lookup_key); it != _pipeline_cache.end()) {
    return it->second;
  }

  auto definition = pipeline_definition;
  definition.rasterization_state.cull_mode = lookup_key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::front;

  auto& compiler = graphics_module.compiler();

  const auto request = graphics::compiler::compile_request{
    .path = _base_pipeline,
    .per_stage = {
      {SLANG_STAGE_VERTEX, { .entry_point = "shadow_main" }},
      { SLANG_STAGE_FRAGMENT, { .entry_point = _entry_points.at(lookup_key.alpha) }}
    }
  };

  const auto result = compiler.compile(request);

  auto compiled = graphics::graphics_pipeline::compiled_shaders{_base_pipeline.filename().string(), result.code};
  auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(lookup_key, handle);

  return entry->second;
}

auto static_mesh_shadow_subrenderer::_get_or_create_descriptor_data(const graphics::graphics_pipeline_handle& handle) -> descriptor_data& {
  if (auto it = _descriptor_cache.find(handle); it != _descriptor_cache.end()) {
    return it->second;
  }

  auto [entry, inserted] = _descriptor_cache.emplace(handle, descriptor_data{handle});

  return entry->second;
}

} // namespace sbx::models
