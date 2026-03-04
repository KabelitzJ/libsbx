// SPDX-License-Identifier: MIT
#include <libsbx/models/frustum_culling_task.hpp>

namespace sbx::models {

frustum_culling_task::frustum_culling_task(const std::filesystem::path& shader_path)
: graphics::task{},
  _culled_ranges{},
  _bounds_buffer{},
  _prefix_sum_buffer{},
  _frustum_buffer{},
  _pipeline{shader_path},
  _push_handler{_pipeline} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  constexpr auto initial_bounds_size = 256 * sizeof(local_aabb);
  constexpr auto initial_prefix_sum_size = 256 * sizeof(std::uint32_t);
  constexpr auto frustum_size = sizeof(frustum_planes);

  _bounds_buffer = graphics_module.add_resource<graphics::storage_buffer>(initial_bounds_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _prefix_sum_buffer = graphics_module.add_resource<graphics::storage_buffer>(initial_prefix_sum_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _frustum_buffer = graphics_module.add_resource<graphics::storage_buffer>(frustum_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
}

frustum_culling_task::~frustum_culling_task() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  graphics_module.remove_resource<graphics::storage_buffer>(_bounds_buffer);
  graphics_module.remove_resource<graphics::storage_buffer>(_prefix_sum_buffer);
  graphics_module.remove_resource<graphics::storage_buffer>(_frustum_buffer);

  for (auto& [key, culled] : _culled_ranges) {
    graphics_module.remove_resource<graphics::storage_buffer>(culled.commands_buffer);
    graphics_module.remove_resource<graphics::storage_buffer>(culled.instances_buffer);
  }
}

auto frustum_culling_task::execute(graphics::command_buffer& command_buffer) -> void {
  EASY_FUNCTION();

  auto timer = graphics::scoped_gpu_timer{command_buffer, fmt::format("frustum_culling_task::execute")};

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto& draw_list = renderer.draw_list<models::static_mesh_material_draw_list>("static_mesh_material");

  auto previous_frame_buffers = std::vector<VkBuffer>{};

  for (const auto& [key, culled] : _culled_ranges) {
    auto& cmds = graphics_module.get_resource<graphics::storage_buffer>(culled.commands_buffer);
    auto& inst = graphics_module.get_resource<graphics::storage_buffer>(culled.instances_buffer);

    previous_frame_buffers.push_back(cmds.handle());
    previous_frame_buffers.push_back(inst.handle());
  }

  if (!previous_frame_buffers.empty()) {
    command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
      .buffers = std::move(previous_frame_buffers),
      .src_stage_mask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .src_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
      .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
    });
  }

  const auto camera_node = scene.camera();
  const auto& camera_component = scene.get_component<scenes::camera>(camera_node);
  const auto projection = camera_component.projection();
  const auto view = math::matrix4x4::inverted(scene.world_transform(camera_node));
  const auto view_projection = projection * view;
  const auto camera_frustum = _extract_frustum_planes(view_projection);

  _cull_bucket(command_buffer, bucket::opaque, no_cascade, camera_frustum, draw_list);
  _cull_bucket(command_buffer, bucket::transparent, no_cascade, camera_frustum, draw_list);

  const auto cascade_light_spaces = scene.cascade_light_spaces();

  for (auto cascade = std::uint32_t{0}; cascade < scenes::scene::cascade_count(); ++cascade) {
    const auto cascade_frustum = _extract_frustum_planes(cascade_light_spaces[cascade]);

    _cull_bucket(command_buffer, bucket::shadow, cascade, cascade_frustum, draw_list);
  }

  auto output_buffers = std::vector<VkBuffer>{};

  for (const auto& [key, culled] : _culled_ranges) {
    auto& commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(culled.commands_buffer);
    auto& instances_buffer = graphics_module.get_resource<graphics::storage_buffer>(culled.instances_buffer);

    output_buffers.push_back(commands_buffer.handle());
    output_buffers.push_back(instances_buffer.handle());
  }

  if (!output_buffers.empty()) {
    command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
      .buffers = std::move(output_buffers),
      .src_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
      .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT
    });
  }
}

auto frustum_culling_task::_cull_bucket(graphics::command_buffer& command_buffer, bucket current_bucket, std::uint32_t cascade, const frustum_planes& frustum, static_mesh_material_draw_list& draw_list) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& frustum_buffer = graphics_module.get_resource<graphics::storage_buffer>(_frustum_buffer);
  frustum_buffer.update(&frustum, sizeof(frustum_planes));

  for (auto& [key, entry] : draw_list.ranges(current_bucket)) {
    const auto bounds = _build_bounds(entry);

    if (bounds.empty()) {
      continue;
    }

    const auto command_count = static_cast<std::uint32_t>(bounds.size());

    const auto [prefix_sum, total_instances] = _build_prefix_sum(entry);

    if (total_instances == 0) {
      continue;
    }

    auto& bounds_buffer = graphics_module.get_resource<graphics::storage_buffer>(_bounds_buffer);
    const auto required_bounds_size = bounds.size() * sizeof(local_aabb);

    if (bounds_buffer.size() < required_bounds_size) {
      bounds_buffer.resize(required_bounds_size + required_bounds_size / 2);
    }

    bounds_buffer.update(bounds.data(), required_bounds_size);

    auto& prefix_buffer = graphics_module.get_resource<graphics::storage_buffer>(_prefix_sum_buffer);
    const auto required_prefix_size = prefix_sum.size() * sizeof(std::uint32_t);

    if (prefix_buffer.size() < required_prefix_size) {
      prefix_buffer.resize(required_prefix_size + required_prefix_size / 2);
    }

    prefix_buffer.update(prefix_sum.data(), required_prefix_size);

    auto& input_commands = graphics_module.get_resource<graphics::storage_buffer>(entry.draw_commands_buffer);
    auto& input_instances = graphics_module.get_resource<graphics::storage_buffer>(entry.instance_data_buffer);
    auto& transforms = draw_list.buffer(static_mesh_material_draw_list::transform_data_buffer_name);

    const auto range_key = culled_range_key{current_bucket, key, cascade};
    auto& culled = _get_or_create_culled_range(range_key, input_commands.size(), input_instances.size());
    auto& output_commands = graphics_module.get_resource<graphics::storage_buffer>(culled.commands_buffer);
    auto& output_instances = graphics_module.get_resource<graphics::storage_buffer>(culled.instances_buffer);

    if (output_commands.size() < input_commands.size()) {
      output_commands.resize(input_commands.size());
    }

    if (output_instances.size() < input_instances.size()) {
      output_instances.resize(input_instances.size());
    }

    command_buffer.fill_buffer(output_commands.handle(), 0, output_commands.size(), 0u);

    command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
      .buffers = {output_commands.handle()},
      .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    });

    _pipeline.bind(command_buffer);

    _push_handler.push("input_commands", input_commands.address());
    _push_handler.push("input_instances", input_instances.address());
    _push_handler.push("output_commands", output_commands.address());
    _push_handler.push("output_instances", output_instances.address());
    _push_handler.push("transforms", transforms.address());
    _push_handler.push("bounds", bounds_buffer.address());
    _push_handler.push("prefix_sum", prefix_buffer.address());
    _push_handler.push("frustum", frustum_buffer.address());
    _push_handler.push("command_count", command_count);
    _push_handler.push("instance_count", total_instances);

    _push_handler.bind(command_buffer);

    constexpr auto workgroup_size = 64u;

    const auto group_count = (total_instances + workgroup_size - 1) / workgroup_size;

    _pipeline.dispatch(command_buffer, math::vector3u{group_count, 1, 1});
  }
}

auto frustum_culling_task::culled(bucket bucket, const material_key& key, std::uint32_t cascade) const -> const culled_range_data* {
  const auto range_key = culled_range_key{bucket, key, cascade};

  if (auto entry = _culled_ranges.find(range_key); entry != _culled_ranges.end()) {
    return &entry->second;
  }

  return nullptr;
}

auto frustum_culling_task::_extract_frustum_planes(const math::matrix4x4f& vp) -> frustum_planes {
  auto result = frustum_planes{};

  // Left: row3 + row0
  result.planes[0] = math::vector4f{
    vp[0][3] + vp[0][0],
    vp[1][3] + vp[1][0],
    vp[2][3] + vp[2][0],
    vp[3][3] + vp[3][0]
  };

  // Right: row3 - row0
  result.planes[1] = math::vector4f{
    vp[0][3] - vp[0][0],
    vp[1][3] - vp[1][0],
    vp[2][3] - vp[2][0],
    vp[3][3] - vp[3][0]
  };

  // Bottom: row3 + row1
  result.planes[2] = math::vector4f{
    vp[0][3] + vp[0][1],
    vp[1][3] + vp[1][1],
    vp[2][3] + vp[2][1],
    vp[3][3] + vp[3][1]
  };

  // Top: row3 - row1
  result.planes[3] = math::vector4f{
    vp[0][3] - vp[0][1],
    vp[1][3] - vp[1][1],
    vp[2][3] - vp[2][1],
    vp[3][3] - vp[3][1]
  };

  // Near: row3 + row2
  result.planes[4] = math::vector4f{
    vp[0][3] + vp[0][2],
    vp[1][3] + vp[1][2],
    vp[2][3] + vp[2][2],
    vp[3][3] + vp[3][2]
  };

  // Far: row3 - row2
  result.planes[5] = math::vector4f{
    vp[0][3] - vp[0][2],
    vp[1][3] - vp[1][2],
    vp[2][3] - vp[2][2],
    vp[3][3] - vp[3][2]
  };

  for (auto& plane : result.planes) {
    const auto length = plane.length();

    if (length > 0.0f) {
      plane /= length;
    }
  }

  return result;
}

auto frustum_culling_task::_get_or_create_culled_range(const culled_range_key& key, VkDeviceSize commands_size, VkDeviceSize instances_size) -> culled_range_data& {
  if (auto entry = _culled_ranges.find(key); entry != _culled_ranges.end()) {
    return entry->second;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto culled = culled_range_data{};

  culled.commands_buffer = graphics_module.add_resource<graphics::storage_buffer>(commands_size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  culled.instances_buffer = graphics_module.add_resource<graphics::storage_buffer>(instances_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

  auto [entry, inserted] = _culled_ranges.emplace(key, culled);

  return entry->second;
}

auto frustum_culling_task::_build_bounds(const static_mesh_material_draw_list::bucket_entry& entry) -> std::vector<local_aabb> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto result = std::vector<local_aabb>{};

  for (const auto& [mesh_id, range] : entry.ranges) {
    auto& mesh = assets_module.get_asset<models::mesh>(mesh_id);

    for (auto i = std::uint32_t{0}; i < range.count; ++i) {
      const auto& submesh = mesh.submesh(i);

      const auto center = (submesh.bounds.min() + submesh.bounds.max()) * 0.5f;
      const auto extents = (submesh.bounds.max() - submesh.bounds.min()) * 0.5f;

      result.push_back(local_aabb{math::vector4f{center.x(), center.y(), center.z(), 0.0f}, math::vector4f{extents.x(), extents.y(), extents.z(), 0.0f}});
    }
  }

  return result;
}

auto frustum_culling_task::_build_prefix_sum(const static_mesh_material_draw_list::bucket_entry& entry) -> prefix_sum_result {
  auto result = prefix_sum_result{
    .prefix_sum = {},
    .total_instances = 0
  };

  auto command_index = std::uint32_t{0};

  for (const auto& reference : entry.ranges) {
    for (auto i = std::uint32_t{0}; i < reference.range.count; ++i) {
      result.prefix_sum.push_back(result.total_instances);
      result.total_instances += entry.command_instance_counts[command_index];
      ++command_index;
    }
  }

  return result;
}

} // namespace sbx::models