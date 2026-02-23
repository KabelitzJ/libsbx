// SPDX-License-Identifier: MIT
#include <libsbx/models/frustum_culling_task.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components/camera.hpp>

namespace sbx::models {

frustum_cull_task::frustum_cull_task(const std::filesystem::path& path)
: _pipeline{path},
  _push_handler{_pipeline} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _input_commands_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  _bounds_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  _output_commands_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
  _draw_count_buffer = graphics_module.add_resource<graphics::storage_buffer>(sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

frustum_cull_task::~frustum_cull_task() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  graphics_module.remove_resource<graphics::storage_buffer>(_input_commands_buffer);
  graphics_module.remove_resource<graphics::storage_buffer>(_bounds_buffer);
  graphics_module.remove_resource<graphics::storage_buffer>(_output_commands_buffer);
  graphics_module.remove_resource<graphics::storage_buffer>(_draw_count_buffer);
}

auto frustum_cull_task::submit_commands(const std::vector<VkDrawIndexedIndirectCommand>& commands, const std::vector<aabb>& bounds) -> void {
  _command_count = static_cast<std::uint32_t>(commands.size());

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& input_cmd_buf = graphics_module.get_resource<graphics::storage_buffer>(_input_commands_buffer);
  _update_buffer<VkDrawIndexedIndirectCommand>(input_cmd_buf, commands);

  auto& bounds_buf = graphics_module.get_resource<graphics::storage_buffer>(_bounds_buffer);
  _update_buffer<aabb>(bounds_buf, bounds);

  auto& output_cmd_buf = graphics_module.get_resource<graphics::storage_buffer>(_output_commands_buffer);
  _resize_buffer<VkDrawIndexedIndirectCommand>(output_cmd_buf, _command_count);
}

auto frustum_cull_task::execute(graphics::command_buffer& command_buffer) -> void {
  if (_command_count == 0) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();

  const auto camera_node = scene.camera();

  const auto& camera_component = scene.get_component<scenes::camera>(camera_node);
  const auto camera_view = math::matrix4x4::inverted(scene.world_transform(camera_node));

  const auto view_frustum = camera_component.view_frustum(camera_view);
  const auto& frustum_planes = view_frustum.planes();

  auto planes = std::array<math::vector4f, 6>{};

  for (auto i = 0u; i < 6u; ++i) {
    const auto& normal = frustum_planes[i].normal();
    const auto distance = frustum_planes[i].distance();

    planes[i] = math::vector4f{normal.x(), normal.y(), normal.z(), distance};
  }

  auto& draw_count_buf = graphics_module.get_resource<graphics::storage_buffer>(_draw_count_buffer);

  command_buffer.fill_buffer(draw_count_buf, 0, sizeof(std::uint32_t), 0);

  // command_buffer.pipeline_barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

  _pipeline.bind(command_buffer);

  _push_handler.push("frustumPlanes", planes);
  _push_handler.push("commandCount", _command_count);

  _push_handler.bind(command_buffer);

  const auto group_count = (_command_count + 63u) / 64u;

  _pipeline.dispatch(command_buffer, {group_count, 1, 1});

  // command_buffer.pipeline_barrier( VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
}

auto frustum_cull_task::output_command_buffer_handle() const -> graphics::storage_buffer_handle {
  return _output_commands_buffer;
}

auto frustum_cull_task::draw_count_buffer_handle() const -> graphics::storage_buffer_handle {
  return _draw_count_buffer;
}

auto frustum_cull_task::submitted_command_count() const -> std::uint32_t {
  return _command_count;
}

} // namespace sbx::models