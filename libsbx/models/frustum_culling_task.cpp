// SPDX-License-Identifier: MIT
#include <libsbx/models/frustum_culling_task.hpp>

namespace sbx::models {

frustum_culling_task::frustum_culling_task(const std::filesystem::path& shader_path)
: graphics::task{},
  _culled_ranges{},
  _bounds_buffers{},
  _prefix_sum_buffers{},
  _frustum_buffers{},
  _pipeline{shader_path},
  _push_handler{_pipeline} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  constexpr auto initial_bounds_size = std::size_t{256u * sizeof(local_aabb)};
  constexpr auto initial_prefix_sum_size = std::size_t{256u * sizeof(std::uint32_t)};
  constexpr auto initial_frustum_size = std::size_t{8u * sizeof(frustum_planes)};

  for (auto i = std::uint32_t{0}; i < ring_size; ++i) {
    _bounds_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(initial_bounds_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _prefix_sum_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(initial_prefix_sum_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _frustum_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(initial_frustum_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }
}

frustum_culling_task::~frustum_culling_task() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (auto i = std::uint32_t{0}; i < ring_size; ++i) {
    graphics_module.remove_resource<graphics::storage_buffer>(_bounds_buffers[i]);
    graphics_module.remove_resource<graphics::storage_buffer>(_prefix_sum_buffers[i]);
    graphics_module.remove_resource<graphics::storage_buffer>(_frustum_buffers[i]);
  }

  for (auto& [key, culled] : _culled_ranges) {
    for (auto i = std::uint32_t{0}; i < ring_size; ++i) {
      graphics_module.remove_resource<graphics::storage_buffer>(culled.commands_buffers[i]);
      graphics_module.remove_resource<graphics::storage_buffer>(culled.instances_buffers[i]);
    }
  }
}

auto frustum_culling_task::execute(graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("frustum_culling_task::execute");

  auto timer = graphics::scoped_gpu_timer{command_buffer, fmt::format("frustum_culling_task::execute")};

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& renderer = graphics_module.renderer();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();

  auto& draw_list = renderer.draw_list<models::static_mesh_material_draw_list>();

  const auto frame = graphics_module.current_frame();

  auto bounds = std::vector<local_aabb>{};
  auto prefix_sums = std::vector<std::uint32_t>{};
  auto frustums = std::vector<frustum_planes>{};
  auto jobs = std::vector<cull_job>{};

  const auto camera_frustum_index = static_cast<std::uint32_t>(frustums.size());

  const auto& view_projection = _debug_view_projection.value_or(environment.view_projection());
  frustums.push_back(_extract_frustum_planes(view_projection));

  _collect_bucket(bucket::opaque, no_cascade, camera_frustum_index, draw_list, bounds, prefix_sums, jobs);
  _collect_bucket(bucket::transparent, no_cascade, camera_frustum_index, draw_list, bounds, prefix_sums, jobs);

  _collect_bucket(bucket::shadow, no_cascade, camera_frustum_index, draw_list, bounds, prefix_sums, jobs);

  if (jobs.empty()) {
    return;
  }

  auto& bounds_buffer = graphics_module.get_resource<graphics::storage_buffer>(_bounds_buffers[frame]);
  const auto required_bounds_size = bounds.size() * sizeof(local_aabb);

  if (bounds_buffer.size() < required_bounds_size) {
    bounds_buffer.resize(required_bounds_size + required_bounds_size / 2);
  }

  if (required_bounds_size > 0) {
    bounds_buffer.update(bounds.data(), required_bounds_size);
  }

  auto& prefix_buffer = graphics_module.get_resource<graphics::storage_buffer>(_prefix_sum_buffers[frame]);
  const auto required_prefix_size = prefix_sums.size() * sizeof(std::uint32_t);

  if (prefix_buffer.size() < required_prefix_size) {
    prefix_buffer.resize(required_prefix_size + required_prefix_size / 2);
  }

  if (required_prefix_size > 0) {
    prefix_buffer.update(prefix_sums.data(), required_prefix_size);
  }

  auto& frustum_buffer = graphics_module.get_resource<graphics::storage_buffer>(_frustum_buffers[frame]);
  const auto required_frustum_size = frustums.size() * sizeof(frustum_planes);

  if (frustum_buffer.size() < required_frustum_size) {
    frustum_buffer.resize(required_frustum_size + required_frustum_size / 2);
  }

  if (required_frustum_size > 0) {
    frustum_buffer.update(frustums.data(), required_frustum_size);
  }

  auto output_handles = std::vector<VkBuffer>{};
  output_handles.reserve(jobs.size() * 2u);

  for (const auto& job : jobs) {
    auto& out_cmds = graphics_module.get_resource<graphics::storage_buffer>(job.output_commands);
    auto& out_inst = graphics_module.get_resource<graphics::storage_buffer>(job.output_instances);

    output_handles.push_back(out_cmds.handle());
    output_handles.push_back(out_inst.handle());
  }

  command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
    .buffers = output_handles,
    .src_stage_mask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
    .src_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
    .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
  });

  for (const auto& job : jobs) {
    auto& out_cmds = graphics_module.get_resource<graphics::storage_buffer>(job.output_commands);

    command_buffer.fill_buffer(out_cmds.handle(), 0, out_cmds.size(), 0u);
  }

  command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
    .buffers = output_handles,
    .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
    .dst_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dst_access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
  });

  _pipeline.bind(command_buffer);

  const auto bounds_address = bounds_buffer.address();
  const auto prefix_address = prefix_buffer.address();
  const auto frustum_address = frustum_buffer.address();

  for (const auto& job : jobs) {
    auto& in_cmds = graphics_module.get_resource<graphics::storage_buffer>(job.input_commands);
    auto& in_inst = graphics_module.get_resource<graphics::storage_buffer>(job.input_instances);
    auto& out_cmds = graphics_module.get_resource<graphics::storage_buffer>(job.output_commands);
    auto& out_inst = graphics_module.get_resource<graphics::storage_buffer>(job.output_instances);

    _push_handler.push("input_commands", in_cmds.address());
    _push_handler.push("input_instances", in_inst.address());
    _push_handler.push("output_commands", out_cmds.address());
    _push_handler.push("output_instances", out_inst.address());
    _push_handler.push("transforms", job.transforms_address);
    _push_handler.push("bounds", bounds_address + job.bounds_offset_bytes);
    _push_handler.push("prefix_sum", prefix_address + job.prefix_offset_bytes);
    _push_handler.push("frustum", frustum_address + job.frustum_offset_bytes);
    _push_handler.push("command_count", job.command_count);
    _push_handler.push("instance_count", job.instance_count);

    _push_handler.bind(command_buffer);

    constexpr auto workgroup_size = 64u;

    const auto group_count = (job.instance_count + workgroup_size - 1u) / workgroup_size;

    _pipeline.dispatch(command_buffer, math::vector3u{group_count, 1u, 1u});
  }

  command_buffer.buffer_barrier(graphics::command_buffer::buffer_barrier_data{
    .buffers = std::move(output_handles),
    .src_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    .dst_stage_mask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
    .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT
  });
}

auto frustum_culling_task::_collect_bucket(bucket current_bucket, std::uint32_t cascade, std::uint32_t frustum_index, static_mesh_material_draw_list& draw_list, std::vector<local_aabb>& bounds, std::vector<std::uint32_t>& prefix_sums, std::vector<cull_job>& jobs) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& transforms_buffer = draw_list.buffer(static_mesh_material_draw_list::transform_data_buffer_name);
  const auto transforms_address = transforms_buffer.address();

  const auto frame = graphics_module.current_frame();

  for (auto& [key, entry] : draw_list.ranges(current_bucket)) {
    auto job_bounds = _build_bounds(entry);

    if (job_bounds.empty()) {
      continue;
    }

    const auto command_count = static_cast<std::uint32_t>(job_bounds.size());

    auto [job_prefix, total_instances] = _build_prefix_sum(entry);

    if (total_instances == 0) {
      continue;
    }

    auto& input_commands = graphics_module.get_resource<graphics::storage_buffer>(entry.draw_commands_buffer);
    auto& input_instances = graphics_module.get_resource<graphics::storage_buffer>(entry.instance_data_buffer);

    const auto range_key = culled_range_key{current_bucket, key, cascade};
    auto& culled = _get_or_create_culled_range(range_key, input_commands.size(), input_instances.size());

    for (auto i = std::uint32_t{0}; i < ring_size; ++i) {
      auto& out_cmds = graphics_module.get_resource<graphics::storage_buffer>(culled.commands_buffers[i]);
      auto& out_inst = graphics_module.get_resource<graphics::storage_buffer>(culled.instances_buffers[i]);

      if (out_cmds.size() < input_commands.size()) {
        out_cmds.resize(input_commands.size());
      }

      if (out_inst.size() < input_instances.size()) {
        out_inst.resize(input_instances.size());
      }
    }

    auto job = cull_job{};
    job.input_commands = entry.draw_commands_buffer;
    job.input_instances = entry.instance_data_buffer;
    job.output_commands = culled.commands_buffers[frame];
    job.output_instances = culled.instances_buffers[frame];
    job.transforms_address = transforms_address;
    job.bounds_offset_bytes = static_cast<std::uint32_t>(bounds.size() * sizeof(local_aabb));
    job.prefix_offset_bytes = static_cast<std::uint32_t>(prefix_sums.size() * sizeof(std::uint32_t));
    job.frustum_offset_bytes = static_cast<std::uint32_t>(frustum_index * sizeof(frustum_planes));
    job.command_count = command_count;
    job.instance_count = total_instances;

    bounds.insert(bounds.end(), job_bounds.begin(), job_bounds.end());
    prefix_sums.insert(prefix_sums.end(), job_prefix.begin(), job_prefix.end());

    jobs.push_back(job);
  }
}

auto frustum_culling_task::culled(bucket bucket, const material_key& key, std::uint32_t cascade) const -> std::optional<culled_range_view> {
  const auto range_key = culled_range_key{bucket, key, cascade};

  if (auto entry = _culled_ranges.find(range_key); entry != _culled_ranges.end()) {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    const auto frame = graphics_module.current_frame();

    return culled_range_view{entry->second.commands_buffers[frame], entry->second.instances_buffers[frame]};
  }

  return std::nullopt;
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

  // Near (Vulkan clip z in [0,1]): row2
  result.planes[4] = math::vector4f{
    vp[0][2],
    vp[1][2],
    vp[2][2],
    vp[3][2]
  };

  // Far: row3 - row2
  result.planes[5] = math::vector4f{
    vp[0][3] - vp[0][2],
    vp[1][3] - vp[1][2],
    vp[2][3] - vp[2][2],
    vp[3][3] - vp[3][2]
  };

  for (auto& plane : result.planes) {
    const auto n = std::sqrt(plane.x() * plane.x() + plane.y() * plane.y() + plane.z() * plane.z());

    if (n > 0.0f) {
      plane /= n;
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

  for (auto i = std::uint32_t{0}; i < ring_size; ++i) {
    culled.commands_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(commands_size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    culled.instances_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(instances_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

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

      result.push_back(local_aabb{math::vector4f{center.x(), center.y(), center.z(), 0.0f}, math::vector4f{extents.x(), extents.y(), extents.z(), 0.0f} * 2.0f});
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
