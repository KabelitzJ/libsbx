// SPDX-License-Identifier: MIT
#include <libsbx/animations/skinned_mesh_subrenderer.hpp>

namespace sbx::animations {

skinned_mesh_subrenderer::skinned_mesh_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const skinned_mesh_material_draw_list::bucket bucket) 
: graphics::subrenderer{},
  _attachments{attachments},
  _base_pipeline{base_pipeline}, 
  _bucket{bucket},
  _skinning_pipeline{"res://shaders/skinning"},
  _skinning_pipeline_push_handler{_skinning_pipeline} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  
  _skinning_vertex_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _skinning_jobs_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
}

skinned_mesh_subrenderer::~skinned_mesh_subrenderer() {
  _pipeline_cache.clear();
}

auto skinned_mesh_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  EASY_FUNCTION();

  SBX_PROFILE_SCOPE("skinned_mesh_subrenderer::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

  // obtain the skinned draw list from the pass (name must match your render graph)
  auto& draw_list = renderer.draw_list<skinned_mesh_material_draw_list>("skinned_mesh_material");

  const auto bone_matrices_buffer_address = draw_list.buffer(skinned_mesh_traits::bone_matrices_buffer_name).address();

  _dispatch_skinning(command_buffer, bone_matrices_buffer_address);

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

    pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::transform_data_buffer_name).address());
    pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::material_data_buffer_name).address());

    // pipeline_data.push_handler.push("bone_matrices_buffer", bone_matrices_buffer_address);

    pipeline_data.push_handler.push("instance_data_buffer", graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer).address());

    pipeline_data.push_handler.push("vertex_buffer", graphics_module.get_resource<graphics::storage_buffer>(_skinning_vertex_buffer).address());

    for (const auto& draw_range : data.ranges) {
      auto& mesh = assets_module.get_asset<animations::mesh>(draw_range.mesh_id);

      mesh.bind(command_buffer);

      pipeline_data.push_handler.bind(command_buffer);

      auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

      command_buffer.draw_indexed_indirect(draw_commands_buffer, draw_range.range.offset, draw_range.range.count);
    }
  }
}

skinned_mesh_subrenderer::pipeline_data::pipeline_data(const graphics::graphics_pipeline_handle& handle)
: pipeline{handle},
  push_handler{pipeline}, 
  scene_descriptor_handler{pipeline, 0u} { }

auto skinned_mesh_subrenderer::_get_or_create_pipeline(const models::material_key& key) -> pipeline_data& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (auto it = _pipeline_cache.find(key); it != _pipeline_cache.end()) {
    return it->second;
  }

  auto definition = pipeline_definition;
  definition.depth = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend) ? graphics::depth::read_only : graphics::depth::read_write;
  definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back;
  definition.uses_transparency = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend);

  auto& compiler = graphics_module.compiler();

  const auto request = graphics::compiler::compile_request{
    .path = _base_pipeline,
    .per_stage = {
      {SLANG_STAGE_VERTEX, {.entry_point = "main"}},
      {SLANG_STAGE_FRAGMENT, {.entry_point = _fs_entry.at(key.alpha)}}
    }
  };

  const auto result = compiler.compile(request);

  auto compiled = graphics::graphics_pipeline::compiled_shaders{ _base_pipeline.filename().string(), result.code };
  auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(key, handle);

  return entry->second;
}

auto skinned_mesh_subrenderer::_dispatch_skinning(graphics::command_buffer& command_buffer, graphics::buffer::address_type bone_matrices_buffer_address) -> void {
  constexpr auto threads_per_group = std::uint32_t{64};

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& jobs = skinned_mesh_traits::skinning_jobs();
  const auto job_count = static_cast<std::uint32_t>(jobs.size());

  if (jobs.empty()) {
    return;
  }

  auto total_vertices = std::uint32_t{0};
  auto max_vertex_count = std::uint32_t{0};

  for (const auto& job : jobs) {
    total_vertices += job.vertex_count;
    max_vertex_count = std::max(max_vertex_count, job.vertex_count);
  }

  auto& skinning_vertex_buffer = graphics_module.get_resource<graphics::storage_buffer>(_skinning_vertex_buffer);

  _resize_buffer<models::vertex3d>(skinning_vertex_buffer, total_vertices);

  auto address = skinning_vertex_buffer.address();

  for (auto& job : jobs) {
    job.post_vertices = address;
    address += job.vertex_count * sizeof(models::vertex3d);
  }

  auto& skinning_jobs_buffer = graphics_module.get_resource<graphics::storage_buffer>(_skinning_jobs_buffer);

  _update_buffer(skinning_jobs_buffer, jobs);

  _skinning_pipeline.bind(command_buffer);

  _skinning_pipeline_push_handler.push("skinning_jobs", skinning_jobs_buffer.address());
  _skinning_pipeline_push_handler.push("bone_matrices_buffer", bone_matrices_buffer_address);

  _skinning_pipeline_push_handler.bind(command_buffer);

  const auto groups_per_job = (max_vertex_count + threads_per_group - 1) / threads_per_group;

  _skinning_pipeline.dispatch(command_buffer, {job_count, groups_per_job, 1});

  auto barrier_data = graphics::command_buffer::buffer_barrier_data{
    .buffers = {
      skinning_vertex_buffer
    },
    .src_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    .dst_stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
    .dst_access_mask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
  };

  command_buffer.buffer_barrier(barrier_data);
}

} // namespace sbx::animations