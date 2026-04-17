// SPDX-License-Identifier: MIT
#include <libsbx/animations/skinning_task.hpp>

#include <libsbx/animations/skinned_mesh_material_subrenderer.hpp>

namespace sbx::animations {

skinning_task::skinning_task(const std::filesystem::path& path)
: graphics::task{},
  _pipeline{path},
  _push_handler{_pipeline} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _vertex_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _jobs_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
}

skinning_task::~skinning_task() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_vertex_buffer.is_valid()) {
    graphics_module.remove_resource<graphics::storage_buffer>(_vertex_buffer);
    _vertex_buffer = {};
  }

  if (_jobs_buffer.is_valid()) {
    graphics_module.remove_resource<graphics::storage_buffer>(_jobs_buffer);
    _jobs_buffer = {};
  }
}

auto skinning_task::vertex_buffer_handle() const -> graphics::storage_buffer_handle {
  return _vertex_buffer;
}

auto skinning_task::execute(graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("skinning_task::execute");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& draw_list = renderer.draw_list<skinned_mesh_material_draw_list>("skinned_mesh_material");

  const auto bone_matrices_buffer_address = draw_list.buffer(skinned_mesh_traits::bone_matrices_buffer_name).address();

  _dispatch_skinning(command_buffer, bone_matrices_buffer_address);
}

auto skinning_task::_dispatch_skinning(graphics::command_buffer& command_buffer, graphics::buffer::address_type bone_matrices_buffer_address) -> void {
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

  auto& skinning_vertex_buffer = graphics_module.get_resource<graphics::storage_buffer>(_vertex_buffer);

  _resize_buffer<models::vertex3d>(skinning_vertex_buffer, total_vertices);

  auto address = skinning_vertex_buffer.address();

  for (auto& job : jobs) {
    job.post_vertices = address;
    address += job.vertex_count * sizeof(models::vertex3d);
  }

  auto& skinning_jobs_buffer = graphics_module.get_resource<graphics::storage_buffer>(_jobs_buffer);

  _update_buffer(skinning_jobs_buffer, jobs);

  _pipeline.bind(command_buffer);

  _push_handler.push("skinning_jobs", skinning_jobs_buffer.address());
  _push_handler.push("bone_matrices_buffer", bone_matrices_buffer_address);

  _push_handler.bind(command_buffer);

  const auto groups_per_job = (max_vertex_count + threads_per_group - 1) / threads_per_group;

  _pipeline.dispatch(command_buffer, {job_count, groups_per_job, 1});

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
