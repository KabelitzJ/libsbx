// SPDX-License-Identifier: MIT
#include <libsbx/models/static_mesh_material_subrenderer.hpp>

namespace sbx::models {

static_mesh_material_subrenderer::static_mesh_material_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const static_mesh_material_draw_list::bucket bucket)
: graphics::subrenderer{},
  _attachments{attachments},
  _base_pipeline{base_pipeline},
  _bucket{bucket} { }

static_mesh_material_subrenderer::~static_mesh_material_subrenderer() {
  _pipeline_cache.clear();
}

auto static_mesh_material_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  EASY_FUNCTION();

  SBX_PROFILE_SCOPE("static_mesh_material_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto& draw_list = renderer.draw_list<models::static_mesh_material_draw_list>("static_mesh_material");

  for (auto& [key, data] : draw_list.ranges(_bucket)) {
    auto& pipeline_data = _get_or_create_pipeline(key);

    auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

    pipeline.bind(command_buffer);

    pipeline_data.scene_descriptor_handler.push("scene", scene.uniform_handler());
    pipeline_data.scene_descriptor_handler.push("samplers", draw_list.samplers());
    pipeline_data.scene_descriptor_handler.push("images", draw_list.images());

    if (!pipeline_data.scene_descriptor_handler.update(pipeline)) {
      return;
    }

    pipeline_data.scene_descriptor_handler.bind_descriptors(command_buffer);

    
    pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(static_mesh_material_draw_list::transform_data_buffer_name).address());
    pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(static_mesh_material_draw_list::material_data_buffer_name).address());

    auto& instance_data_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer);

    pipeline_data.push_handler.push("instance_data_buffer", instance_data_buffer.address());

    auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

    for (const auto& [mesh_id, range] : data.ranges) {
      auto& mesh = assets_module.get_asset<models::mesh>(mesh_id);
      
      mesh.bind(command_buffer);
      
      pipeline_data.push_handler.push("vertex_buffer", mesh.address());

      pipeline_data.push_handler.bind(command_buffer);

      command_buffer.draw_indexed_indirect(draw_commands_buffer, range.offset, range.count);
    }
  }
}

auto static_mesh_material_subrenderer::_get_or_create_pipeline(const material_key& key) -> pipeline_data& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (auto entry = _pipeline_cache.find(key); entry != _pipeline_cache.end()) {
    return entry->second;
  }

  auto definition = pipeline_definition;
  definition.depth = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend) ? graphics::depth::read_only : graphics::depth::read_write;
  definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back;
  definition.uses_transparency = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend);

  auto& compiler = graphics_module.compiler();

  const auto request = graphics::compiler::compile_request{
    .path = _base_pipeline,
    .per_stage = {
      {SLANG_STAGE_VERTEX, { .entry_point = "main" }},
      {SLANG_STAGE_FRAGMENT, { .entry_point = _entry_point.at(key.alpha) }}
    }
  };

  const auto result = compiler.compile(request);

  auto compiled_shaders = graphics::graphics_pipeline::compiled_shaders{_base_pipeline.filename().string(), result.code};

  auto pipeline = graphics_module.add_resource<graphics::graphics_pipeline>(compiled_shaders, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(key, pipeline);

  return entry->second;
}

} // namespace sbx::models