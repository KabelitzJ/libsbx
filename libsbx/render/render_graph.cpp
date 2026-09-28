// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_graph.hpp>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>

namespace sbx::render {

inline constexpr auto pipeline_statistics_flags = VkQueryPipelineStatisticFlags{
  VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
  VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
  VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
  VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
};

namespace detail {

auto resource_builder::reads_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::read_image, .group_index = group_index, .image = image, .stage = stage, .access = access, .layout = layout});
}

auto resource_builder::writes_image(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::write_image, .group_index = group_index, .image = image, .stage = stage, .access = access, .layout = layout});
}

auto resource_builder::reads_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::read_buffer, .group_index = group_index, .buffer = buffer, .stage = stage, .access = access});
}

auto resource_builder::writes_buffer(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access, std::uint32_t group_index) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::write_buffer, .group_index = group_index, .buffer = buffer, .stage = stage, .access = access});
}

auto resource_builder::declares_image_ready(graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout, std::uint32_t group_index) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::declare_image_ready, .group_index = group_index, .image = image, .stage = stage, .access = access, .layout = layout});
}

} // namespace detail

auto graphics_pass_builder::add_group(const render_attachment_group& group) -> std::uint32_t {
  const auto index = _group_count++;
  _group_extents.push_back(group.extent);

  for (const auto& color : group.colors) {
    _operations.push_back(detail::recorded_operation{
      .kind = detail::operation_kind::color_attachment,
      .group_index = index,
      .image = color.image,
      .resolve_image = color.resolve_image,
      .stage = color.stage_mask,
      .access = color.access_mask,
      .layout = graphics::image_layout::color_attachment_optimal,
      .store_op = color.store_op,
      .clear_color = color.clear_value
    });
  }

  if (group.depth) {
    _operations.push_back(detail::recorded_operation{
      .kind = detail::operation_kind::depth_attachment,
      .group_index = index,
      .image = group.depth->image,
      .resolve_image = group.depth->resolve_image,
      .resolve_mode = group.depth->resolve_mode,
      .stage = group.depth->stage_mask,
      .access = group.depth->access_mask,
      .layout = graphics::image_layout::depth_attachment_optimal,
      .store_op = group.depth->store_op,
      .clear_depth = group.depth->clear_value
    });
  }

  return index;
}

auto graphics_pass_builder::transitions_after(std::uint32_t group_index, graphics::image_handle image, graphics::pipeline_stage dst_stage, graphics::access dst_access, graphics::image_layout dst_layout) -> void {
  _operations.push_back(detail::recorded_operation{
    .kind = detail::operation_kind::transition_after,
    .group_index = group_index,
    .image = image,
    .stage = dst_stage,
    .access = dst_access,
    .layout = dst_layout
  });
}

/**
 * @brief Per-resource sync state the linear compiler tracks while walking the pass list in order.
 *
 * write_stage/write_access: the last write (or layout transition, or declared-ready point) that
 * every later access waits on. read_stages/read_accesses: every read since then that has already
 * been made to wait on it -- a later read in a stage/access not covered yet still needs its own
 * barrier, and a later write has to wait on all of them, not only on the most recent one.
 *
 * touched=false means the first declared access this frame, so the synthesized barrier is a fresh
 * write (old_layout=undefined) that waits on the carried end-of-previous-frame scope.
 */
struct sync_scope {
  graphics::pipeline_stage stage;
  graphics::access access;
}; // struct sync_scope

struct sync_state {
  // How the previous frame left this resource (see compile()); the first barrier this frame waits on it.
  std::optional<sync_scope> carried{};
  graphics::pipeline_stage write_stage{graphics::pipeline_stage::none};
  graphics::access write_access{graphics::access::none};
  graphics::pipeline_stage read_stages{graphics::pipeline_stage::none};
  graphics::access read_accesses{graphics::access::none};
  bool touched{false};
}; // struct sync_state

struct image_state : sync_state {
  graphics::image_layout layout{graphics::image_layout::undefined};
}; // struct image_state

using buffer_state = sync_state;

// What a later frame's first access to the resource has to wait on.
[[nodiscard]] auto end_of_frame_scope(const sync_state& state) -> sync_scope {
  return sync_scope{state.write_stage | state.read_stages, state.write_access};
}

struct declared_pass {
  std::vector<detail::recorded_operation> operations;
  std::vector<math::vector2u> group_extents;
}; // struct declared_pass

inline constexpr auto write_access_mask =
  graphics::access::shader_write |
  graphics::access::shader_storage_write |
  graphics::access::color_attachment_write |
  graphics::access::depth_stencil_attachment_write |
  graphics::access::transfer_write |
  graphics::access::host_write |
  graphics::access::memory_write;

[[nodiscard]] auto is_write_access(graphics::access access) noexcept -> bool {
  return (access & write_access_mask) != graphics::access::none;
}

// The source scope the barrier in front of this access needs, or nullopt when an earlier barrier
// already covers it (a read in a stage/access that has already waited on the last write). Advances
// state past the access. is_transition marks a layout change, which counts as a write.
[[nodiscard]] auto advance_sync(sync_state& state, graphics::pipeline_stage stage, graphics::access access, bool is_transition) -> std::optional<sync_scope> {
  const auto writes = is_write_access(access);
  const auto is_write = writes || is_transition;

  auto result = std::optional<sync_scope>{};

  if (!state.touched) {
    result = state.carried.value_or(sync_scope{stage, graphics::access::none});
  } else if (is_write) {
    result = sync_scope{state.write_stage | state.read_stages, state.write_access};
  } else if ((state.read_stages & stage) != stage || (state.read_accesses & access) != access) {
    result = sync_scope{state.write_stage, state.write_access};
  }

  if (is_write || !state.touched) {
    // A pure read that transitioned (or first-touched) the resource: the barrier just emitted is
    // the new point later accesses chain from, and this read already waited on it.
    state.write_stage = stage;
    state.write_access = writes ? access : graphics::access::none;
    state.read_stages = writes ? graphics::pipeline_stage::none : stage;
    state.read_accesses = writes ? graphics::access::none : access;
  } else {
    state.read_stages = state.read_stages | stage;
    state.read_accesses = state.read_accesses | access;
  }

  state.touched = true;

  return result;
}

// What a pass's declares_*_ready promises: it made the resource visible to (stage, access) itself.
auto mark_ready(sync_state& state, graphics::pipeline_stage stage, graphics::access access) -> void {
  state.write_stage = stage;
  state.write_access = graphics::access::none;
  state.read_stages = stage;
  state.read_accesses = access;
  state.touched = true;
}

auto render_graph::compile(const graph_resources& resources) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  // Every pass declares once (declare() may have side effects, e.g. bloom_pass rebuilding its views).
  auto declared = std::vector<declared_pass>{};
  declared.reserve(_passes.size());

  for (auto& pass : _passes) {
    if (pass->kind() == pass_kind::graphics) {
      auto builder = graphics_pass_builder{};
      pass->declare_resources(&builder, nullptr, resources);

      auto extents = std::vector<math::vector2u>{};

      for (auto group_index = std::uint32_t{0u}; group_index < builder.group_count(); ++group_index) {
        extents.push_back(builder.group_extent(group_index));
      }

      declared.push_back(declared_pass{builder.operations(), std::move(extents)});
    } else {
      auto builder = compute_pass_builder{};
      pass->declare_resources(nullptr, &builder, resources);

      declared.push_back(declared_pass{builder.operations(), {}});
    }
  }

  // The declared operations are walked twice. Round 0 only finds the state every resource is left
  // in at the end of a frame; round 1 compiles for real, with each resource's first barrier waiting
  // on that end state -- the previous frame's last accesses, which can still be executing with
  // frames in flight (every render target is shared across them). Buffers aren't carried: every
  // graph-tracked buffer has one region per frame slot (see resource_builder::reads_buffer), so the
  // previous frame never touches what this frame does.
  auto carried_images = std::unordered_map<graphics::image_handle, sync_scope>{};

  for (auto round = 0u; round < 2u; ++round) {
    _compiled.clear();

    auto image_states = std::unordered_map<graphics::image_handle, image_state>{};
    auto buffer_states = std::unordered_map<graphics::buffer_handle, buffer_state>{};

    for (const auto& [image, scope] : carried_images) {
      image_states[image].carried = scope;
    }

    // Image -> (entry index, group index) of the group whose first use cleared it. An entry index of
    // _compiled.size() means the entry still being built (not yet pushed).
    auto cleared_by = std::unordered_map<graphics::image_handle, std::pair<std::size_t, std::uint32_t>>{};

    const auto record_clear = [&](graphics::image_handle image, std::uint32_t group_index) -> void {
      cleared_by[image] = {_compiled.size(), group_index};
    };

    const auto note_dependent_use = [&](compiled_entry& current, graphics::image_handle image) -> void {
      if (const auto clearer = cleared_by.find(image); clearer != cleared_by.end()) {
        const auto [entry_index, group_index] = clearer->second;
        auto& owner = (entry_index == _compiled.size()) ? current : _compiled[entry_index];
        owner.groups[group_index].clear_on_skip = true;
      }
    };

    // Read-after-read in an already-covered stage stays barrier-free (e.g. transparent_accumulate_pass's
    // depth read) -- see advance_sync().
    const auto touch_image = [&](graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout) -> std::optional<graphics::command_buffer::image_transition_data> {
      auto& state = image_states[image];

      const auto old_layout = state.touched ? state.layout : graphics::image_layout::undefined;
      const auto scope = advance_sync(state, stage, access, state.touched && state.layout != layout);

      state.layout = layout;

      if (!scope) {
        return std::nullopt;
      }

      auto& img = registry.get<graphics::image>(image);

      auto data = graphics::command_buffer::image_transition_data{};
      data.image = img.handle();
      data.src_stage_mask = graphics::to_vk_enum<VkPipelineStageFlags2>(scope->stage);
      data.src_access_mask = graphics::to_vk_enum<VkAccessFlags2>(scope->access);
      data.dst_stage_mask = graphics::to_vk_enum<VkPipelineStageFlags2>(stage);
      data.dst_access_mask = graphics::to_vk_enum<VkAccessFlags2>(access);
      data.old_layout = old_layout;
      data.new_layout = layout;
      data.aspect_mask = img.aspect();
      data.layer_count = 1u;

      return data;
    };

    const auto touch_buffer = [&](graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access) -> std::optional<VkMemoryBarrier2> {
      const auto scope = advance_sync(buffer_states[buffer], stage, access, false);

      if (!scope) {
        return std::nullopt;
      }

      auto data = VkMemoryBarrier2{};
      data.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      data.srcStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(scope->stage);
      data.srcAccessMask = graphics::to_vk_enum<VkAccessFlags2>(scope->access);
      data.dstStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(stage);
      data.dstAccessMask = graphics::to_vk_enum<VkAccessFlags2>(access);

      return data;
    };

    const auto apply_op = [&](compiled_entry& entry, const detail::recorded_operation& op) -> void {
      using detail::operation_kind;

      switch (op.kind) {
        case operation_kind::color_attachment: {
          auto& group = entry.groups[op.group_index];
          group.has_rendering = true;

          // First touch this compile clears (to op.clear_color); later touches load. Peeked before
          // touch_image, which is what actually marks it touched.
          const auto is_first_use = !image_states[op.image].touched;

          if (is_first_use) {
            record_clear(op.image, op.group_index);
          } else {
            note_dependent_use(entry, op.image);
          }

          if (const auto barrier = touch_image(op.image, op.stage, op.access, op.layout)) {
            group.entry_image_barriers.push_back(*barrier);
          }

          auto& image = registry.get<graphics::image>(op.image);

          auto info = VkRenderingAttachmentInfo{};
          info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          info.imageView = image.view();
          info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          info.loadOp = is_first_use ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
          info.storeOp = graphics::to_vk_enum<VkAttachmentStoreOp>(op.store_op);
          info.clearValue.color = VkClearColorValue{{op.clear_color.r(), op.clear_color.g(), op.clear_color.b(), op.clear_color.a()}};

          if (op.resolve_image.is_valid()) {
            if (!image_states[op.resolve_image].touched) {
              record_clear(op.resolve_image, op.group_index); // first written by this group's resolve
            }

            if (const auto resolve_barrier = touch_image(op.resolve_image, op.stage, op.access, op.layout)) {
              group.entry_image_barriers.push_back(*resolve_barrier);
            }

            auto& resolve_image = registry.get<graphics::image>(op.resolve_image);

            info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            info.resolveImageView = resolve_image.view();
            info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          }

          group.color_attachments[group.color_attachment_count++] = info;

          break;
        }

        case operation_kind::depth_attachment: {
          auto& group = entry.groups[op.group_index];
          group.has_rendering = true;

          const auto is_first_use = !image_states[op.image].touched;

          if (is_first_use) {
            record_clear(op.image, op.group_index);
          } else {
            note_dependent_use(entry, op.image);
          }

          if (const auto barrier = touch_image(op.image, op.stage, op.access, op.layout)) {
            group.entry_image_barriers.push_back(*barrier);
          }

          auto& image = registry.get<graphics::image>(op.image);

          auto info = VkRenderingAttachmentInfo{};
          info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          info.imageView = image.view();
          info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
          info.loadOp = is_first_use ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
          info.storeOp = graphics::to_vk_enum<VkAttachmentStoreOp>(op.store_op);
          info.clearValue.depthStencil = VkClearDepthStencilValue{op.clear_depth.depth, op.clear_depth.stencil};

          if (op.resolve_image.is_valid()) {
            if (!image_states[op.resolve_image].touched) {
              record_clear(op.resolve_image, op.group_index); // first written by this group's resolve
            }

            if (const auto resolve_barrier = touch_image(op.resolve_image, graphics::pipeline_stage::color_attachment_output, graphics::access::color_attachment_write, graphics::image_layout::general)) {
              group.entry_image_barriers.push_back(*resolve_barrier);
            }

            auto& resolve_image = registry.get<graphics::image>(op.resolve_image);

            info.resolveMode = graphics::to_vk_enum<VkResolveModeFlagBits>(op.resolve_mode);
            info.resolveImageView = resolve_image.view();
            info.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
          }

          group.depth_attachment = info;
          group.has_depth = true;

          break;
        }

        case operation_kind::read_image:
        case operation_kind::write_image: {
          auto& group = entry.groups[op.group_index];

          note_dependent_use(entry, op.image);

          if (const auto barrier = touch_image(op.image, op.stage, op.access, op.layout)) {
            group.entry_image_barriers.push_back(*barrier);
          }

          break;
        }

        case operation_kind::read_buffer:
        case operation_kind::write_buffer: {
          auto& group = entry.groups[op.group_index];

          if (const auto barrier = touch_buffer(op.buffer, op.stage, op.access)) {
            group.entry_buffer_barriers.push_back(*barrier);
          }

          break;
        }

        case operation_kind::declare_image_ready: {
          auto& state = image_states[op.image];
          mark_ready(state, op.stage, op.access);
          state.layout = op.layout;

          break;
        }

        case operation_kind::transition_after: {
          auto& group = entry.groups[op.group_index];

          if (const auto barrier = touch_image(op.image, op.stage, op.access, op.layout)) {
            group.exit_image_barriers.push_back(*barrier);
          }

          break;
        }
      }
    };

    for (auto pass_index = std::size_t{0u}; pass_index < _passes.size(); ++pass_index) {
      const auto& pass = declared[pass_index];

      auto entry = compiled_entry{memory::make_observer(_passes[pass_index].get()), {}};

      entry.groups.resize(std::max(pass.group_extents.size(), std::size_t{1u}));

      for (auto group_index = std::size_t{0u}; group_index < pass.group_extents.size(); ++group_index) {
        entry.groups[group_index].extent = pass.group_extents[group_index];
      }

      for (const auto& op : pass.operations) {
        apply_op(entry, op);
      }

      _compiled.push_back(std::move(entry));
    }

    if (round == 0u) {
      for (const auto& [image, state] : image_states) {
        carried_images[image] = end_of_frame_scope(state);
      }
    }
  }
}

auto render_graph::initialize_gpu_queries(const graphics::physical_device& physical_device, const graphics::logical_device& logical_device) -> void {
  _timestamp_period_ns = physical_device.properties().limits.timestampPeriod;

  const auto pass_count = static_cast<std::uint32_t>(_passes.size());
  const auto timestamp_count = pass_count * 2u * graphics::swapchain::max_frames_in_flight;

  _timestamp_pool = std::make_unique<graphics::query_pool>(logical_device, VK_QUERY_TYPE_TIMESTAMP, timestamp_count);

  if (logical_device.enabled_features().core().pipelineStatisticsQuery) {
    _pipeline_stats_pool = std::make_unique<graphics::query_pool>(logical_device, VK_QUERY_TYPE_PIPELINE_STATISTICS, graphics::swapchain::max_frames_in_flight, pipeline_statistics_flags);
  }

  _pass_timings.resize(pass_count);

  for (auto index = std::size_t{0u}; index < _passes.size(); ++index) {
    _pass_timings[index].name = _passes[index]->name();
  }
}

auto render_graph::execute(render_context& context) -> void {
  const auto pass_count = static_cast<std::uint32_t>(_passes.size());
  const auto has_gpu_queries = _timestamp_pool && _pipeline_stats_pool;

  // (slot * pass_count + pass_index) * 2 + (is_end ? 1 : 0) -- see initialize_gpu_queries's own
  // sizing comment for why this range never needs to grow after construction.
  const auto timestamp_base = context.slot * pass_count * 2u;

  if (has_gpu_queries) {
    context.command_buffer->reset_query_pool(*_timestamp_pool, timestamp_base, pass_count * 2u);
    context.command_buffer->reset_query_pool(*_pipeline_stats_pool, context.slot, 1u);
    context.command_buffer->begin_query(*_pipeline_stats_pool, context.slot);
  }

  for (auto pass_index = std::size_t{0u}; pass_index < _compiled.size(); ++pass_index) {
    auto& entry = _compiled[pass_index];

    auto wrote_begin_timestamp = false;

    for (auto group_index = std::uint32_t{0u}; group_index < entry.groups.size(); ++group_index) {
      const auto& group = entry.groups[group_index];

      // A disabled group still issues its barriers: compile() tracked every later barrier's
      // old_layout/src scope assuming these ran, so skipping them would desync the real layouts.
      // It also still runs its rendering scope (with nothing drawn) when a later group depends on a
      // clear it performs -- see compiled_group::clear_on_skip.
      const auto is_enabled = entry.pass->is_group_enabled(context, group_index);
      const auto runs_rendering = group.has_rendering && (is_enabled || group.clear_on_skip);

      if (is_enabled && has_gpu_queries && !wrote_begin_timestamp) {
        context.command_buffer->write_timestamp(*_timestamp_pool, timestamp_base + static_cast<std::uint32_t>(pass_index) * 2u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
        wrote_begin_timestamp = true;
      }

      for (const auto& barrier : group.entry_image_barriers) {
        context.command_buffer->transition_image_layout(barrier);
      }

      for (const auto& barrier : group.entry_buffer_barriers) {
        context.command_buffer->memory_dependency(barrier);
      }

      if (runs_rendering) {
        auto rendering_info = VkRenderingInfo{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{group.extent.x(), group.extent.y()}};
        rendering_info.layerCount = 1u;
        rendering_info.colorAttachmentCount = group.color_attachment_count;
        rendering_info.pColorAttachments = group.color_attachment_count > 0u ? group.color_attachments.data() : nullptr;
        rendering_info.pDepthAttachment = group.has_depth ? &group.depth_attachment : nullptr;

        context.command_buffer->begin_rendering(rendering_info);
      }

      if (is_enabled) {
        entry.pass->execute_group(context, group_index);
      }

      if (runs_rendering) {
        context.command_buffer->end_rendering();
      }

      for (const auto& barrier : group.exit_image_barriers) {
        context.command_buffer->transition_image_layout(barrier);
      }
    }

    if (has_gpu_queries && wrote_begin_timestamp) {
      context.command_buffer->write_timestamp(*_timestamp_pool, timestamp_base + static_cast<std::uint32_t>(pass_index) * 2u + 1u, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    }
  }

  if (!has_gpu_queries) {
    return;
  }

  context.command_buffer->end_query(*_pipeline_stats_pool, context.slot);

  ++_frames_executed;

  if (_frames_executed <= graphics::swapchain::max_frames_in_flight) {
    return;
  }

  auto raw_timestamps = std::vector<std::uint64_t>{};

  for (auto pass_index = std::size_t{0u}; pass_index < _pass_timings.size(); ++pass_index) {
    if (!_timestamp_pool->try_read_results(timestamp_base + static_cast<std::uint32_t>(pass_index) * 2u, 2u, raw_timestamps)) {
      continue;
    }

    const auto begin_ticks = raw_timestamps[0];
    const auto end_ticks = raw_timestamps[1];

    const auto elapsed_ticks = end_ticks >= begin_ticks ? (end_ticks - begin_ticks) : std::uint64_t{0u};

    _pass_timings[pass_index].milliseconds = static_cast<std::float_t>(elapsed_ticks) * _timestamp_period_ns / 1.0e6f;
  }

  auto raw_stats = std::vector<std::uint64_t>{};

  if (_pipeline_stats_pool->try_read_results(context.slot, 1u, raw_stats) && raw_stats.size() >= 6u) {
    _pipeline_stats.input_assembly_vertices = raw_stats[0];
    _pipeline_stats.input_assembly_primitives = raw_stats[1];
    _pipeline_stats.vertex_shader_invocations = raw_stats[2];
    _pipeline_stats.clipping_invocations = raw_stats[3];
    _pipeline_stats.clipping_primitives = raw_stats[4];
    _pipeline_stats.fragment_shader_invocations = raw_stats[5];
  }
}

} // namespace sbx::render
