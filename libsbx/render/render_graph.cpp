// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_graph.hpp>

#include <optional>
#include <unordered_map>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>

namespace sbx::render {

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

auto resource_builder::declares_buffer_ready(graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access) -> void {
  _operations.push_back(recorded_operation{.kind = operation_kind::declare_buffer_ready, .buffer = buffer, .stage = stage, .access = access});
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
 * @brief Per-resource state the linear compiler tracks while walking the pass list in order.
 *
 * touched=false means the first declared access this compile, so the synthesized barrier is a
 * fresh write (old_layout=undefined, src_access=none).
 */
struct image_state {
  graphics::image_layout layout{graphics::image_layout::undefined};
  graphics::pipeline_stage stage{graphics::pipeline_stage::none};
  graphics::access access{graphics::access::none};
  bool last_was_write{false};
  bool touched{false};
}; // struct image_state

struct buffer_state {
  graphics::pipeline_stage stage{graphics::pipeline_stage::none};
  graphics::access access{graphics::access::none};
  bool last_was_write{false};
  bool touched{false};
}; // struct buffer_state

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

auto render_graph::compile(const graph_resources& resources) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  _compiled.clear();

  auto image_states = std::unordered_map<graphics::image_handle, image_state>{};
  auto buffer_states = std::unordered_map<graphics::buffer_handle, buffer_state>{};

  // Emits a barrier when access is a write, layout changes, the previous access was a write, or
  // this is the first touch; otherwise skips it (read-after-read elision, e.g. keeps
  // transparent_accumulate_pass's depth read barrier-free).
  const auto touch_image = [&](graphics::image_handle image, graphics::pipeline_stage stage, graphics::access access, graphics::image_layout layout) -> std::optional<graphics::command_buffer::image_transition_data> {
    auto& state = image_states[image];

    const auto write_now = is_write_access(access);
    const auto needs_barrier = !state.touched || write_now || state.last_was_write || state.layout != layout;

    auto result = std::optional<graphics::command_buffer::image_transition_data>{};

    if (needs_barrier) {
      auto& img = registry.get<graphics::image>(image);

      auto data = graphics::command_buffer::image_transition_data{};
      data.image = img.handle();
      data.src_stage_mask = graphics::to_vk_enum<VkPipelineStageFlags2>(state.touched ? state.stage : stage);
      data.src_access_mask = state.touched ? graphics::to_vk_enum<VkAccessFlags2>(state.access) : VK_ACCESS_2_NONE;
      data.dst_stage_mask = graphics::to_vk_enum<VkPipelineStageFlags2>(stage);
      data.dst_access_mask = graphics::to_vk_enum<VkAccessFlags2>(access);
      data.old_layout = state.touched ? state.layout : graphics::image_layout::undefined;
      data.new_layout = layout;
      data.aspect_mask = img.aspect();
      data.layer_count = 1u;

      result = data;
    }

    state.layout = layout;
    state.stage = stage;
    state.access = access;
    state.last_was_write = write_now;
    state.touched = true;

    return result;
  };

  const auto touch_buffer = [&](graphics::buffer_handle buffer, graphics::pipeline_stage stage, graphics::access access) -> std::optional<VkMemoryBarrier2> {
    auto& state = buffer_states[buffer];

    const auto write_now = is_write_access(access);
    const auto needs_barrier = !state.touched || write_now || state.last_was_write;

    auto result = std::optional<VkMemoryBarrier2>{};

    if (needs_barrier) {
      auto data = VkMemoryBarrier2{};
      data.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      data.srcStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(state.touched ? state.stage : stage);
      data.srcAccessMask = state.touched ? graphics::to_vk_enum<VkAccessFlags2>(state.access) : VK_ACCESS_2_NONE;
      data.dstStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(stage);
      data.dstAccessMask = graphics::to_vk_enum<VkAccessFlags2>(access);

      result = data;
    }

    state.stage = stage;
    state.access = access;
    state.last_was_write = write_now;
    state.touched = true;

    return result;
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

        group.depth_attachment = info;
        group.has_depth = true;

        break;
      }

      case operation_kind::read_image:
      case operation_kind::write_image: {
        auto& group = entry.groups[op.group_index];

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

      case operation_kind::declare_buffer_ready: {
        auto& state = buffer_states[op.buffer];
        state.stage = op.stage;
        state.access = op.access;
        state.last_was_write = false;
        state.touched = true;

        break;
      }

      case operation_kind::declare_image_ready: {
        auto& state = image_states[op.image];
        state.layout = op.layout;
        state.stage = op.stage;
        state.access = op.access;
        state.last_was_write = false;
        state.touched = true;

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

  for (auto& pass : _passes) {
    auto entry = compiled_entry{memory::make_observer(pass.get()), {}};

    if (pass->kind() == pass_kind::graphics) {
      auto builder = graphics_pass_builder{};
      pass->declare_resources(&builder, nullptr, resources);

      entry.groups.resize(std::max(builder.group_count(), std::uint32_t{1u}));

      for (auto group_index = std::uint32_t{0u}; group_index < builder.group_count(); ++group_index) {
        entry.groups[group_index].extent = builder.group_extent(group_index);
      }

      for (const auto& op : builder.operations()) {
        apply_op(entry, op);
      }
    } else {
      auto builder = compute_pass_builder{};
      pass->declare_resources(nullptr, &builder, resources);

      entry.groups.resize(1u);

      for (const auto& op : builder.operations()) {
        apply_op(entry, op);
      }
    }

    _compiled.push_back(std::move(entry));
  }
}

auto render_graph::execute(render_context& context) -> void {
  for (auto& entry : _compiled) {
    for (auto group_index = std::uint32_t{0u}; group_index < entry.groups.size(); ++group_index) {
      if (!entry.pass->is_group_enabled(context, group_index)) {
        continue;
      }

      const auto& group = entry.groups[group_index];

      for (const auto& barrier : group.entry_image_barriers) {
        context.command_buffer->transition_image_layout(barrier);
      }

      for (const auto& barrier : group.entry_buffer_barriers) {
        context.command_buffer->memory_dependency(barrier);
      }

      if (group.has_rendering) {
        auto rendering_info = VkRenderingInfo{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{group.extent.x(), group.extent.y()}};
        rendering_info.layerCount = 1u;
        rendering_info.colorAttachmentCount = group.color_attachment_count;
        rendering_info.pColorAttachments = group.color_attachment_count > 0u ? group.color_attachments.data() : nullptr;
        rendering_info.pDepthAttachment = group.has_depth ? &group.depth_attachment : nullptr;

        context.command_buffer->begin_rendering(rendering_info);
      }

      entry.pass->execute_group(context, group_index);

      if (group.has_rendering) {
        context.command_buffer->end_rendering();
      }

      for (const auto& barrier : group.exit_image_barriers) {
        context.command_buffer->transition_image_layout(barrier);
      }
    }
  }
}

} // namespace sbx::render
