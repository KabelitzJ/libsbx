// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/shadow/local_shadow_pass.hpp>

#include <algorithm>
#include <cmath>

#include <vulkan/vulkan.h>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>

#include <libsbx/graphics/profiler.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/shadow/shadow_pass.hpp>

namespace sbx::render {

local_shadow_pass::local_shadow_pass()
: _pipelines{create_shadow_caster_pipelines("Local Shadow")} { }

auto local_shadow_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  builder.reads_buffer(resources.culled_indirect_args_buffer, graphics::pipeline_stage::draw_indirect, graphics::access::indirect_command_read);
  builder.reads_buffer(resources.culled_transform_buffer, graphics::pipeline_stage::vertex_shader, graphics::access::shader_read);
  builder.reads_buffer(resources.instanced_culled_buffer, graphics::pipeline_stage::vertex_shader, graphics::access::shader_read);
  builder.reads_buffer(resources.skin_scratch_buffer, graphics::pipeline_stage::vertex_shader, graphics::access::shader_read);

  auto group = render_attachment_group{.extent = math::vector2u{local_shadow_atlas_resolution, local_shadow_atlas_resolution}};

  // Tiles whose content didn't change keep last frame's depth, so the atlas is loaded, never cleared as a whole.

  group.depth = depth_attachment_slot{
    .image = resources.local_shadow_atlas,
    .access_mask = graphics::access::depth_stencil_attachment_write | graphics::access::depth_stencil_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = graphics::depth_stencil_clear_value{1.0f, 0u},
    .preserve_contents = true
  };

  builder.add_group(group);
}

auto local_shadow_pass::should_execute(const render_context& context, std::uint32_t /*group*/) const -> bool {
  return std::ranges::any_of(context.packet->local_shadow_dirty, [](const auto dirty) { return dirty != 0u; });
}

auto local_shadow_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("local_shadow_pass::execute");
  SBX_STATS_SCOPE("local_shadow_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "local_shadow_pass::execute");

  const auto atlas_size = static_cast<std::float_t>(local_shadow_atlas_resolution);

  bind_globals(context, math::vector2u{local_shadow_atlas_resolution, local_shadow_atlas_resolution});

  const auto resolver = [this](const material_shader_source& source, bool is_double_sided) {
    return resolve_shadow_caster_pipeline(_pipelines, source, is_double_sided, "Local Shadow");
  };

  const auto& views = context.packet->local_shadow_views;
  const auto& dirty = context.packet->local_shadow_dirty;

  for (auto tile = std::uint32_t{0u}; tile < views.size(); ++tile) {
    if (tile >= dirty.size() || dirty[tile] == 0u) {
      continue;
    }

    const auto& rect = views[tile].atlas_rect;

    const auto x = std::round(rect.x() * atlas_size);
    const auto y = std::round(rect.y() * atlas_size);
    const auto size = std::round(rect.z() * atlas_size);

    const auto scissor = VkRect2D{VkOffset2D{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)}, VkExtent2D{static_cast<std::uint32_t>(size), static_cast<std::uint32_t>(size)}};

    // Only this tile is redrawn, so only this tile is cleared.
    auto clear_attachment = VkClearAttachment{};
    clear_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_attachment.clearValue.depthStencil = VkClearDepthStencilValue{1.0f, 0u};

    auto clear_rect = VkClearRect{};
    clear_rect.rect = scissor;
    clear_rect.baseArrayLayer = 0u;
    clear_rect.layerCount = 1u;

    vkCmdClearAttachments(*context.command_buffer, 1u, &clear_attachment, 1u, &clear_rect);

    context.command_buffer->set_viewport(VkViewport{x, y, size, size, 0.0f, 1.0f});
    context.command_buffer->set_scissor(scissor);

    submit_draw_commands_indirect(context, context.packet->shadow_caster_commands, _pipelines, resolver, shadow_cascade_count + tile);
  }
}

} // namespace sbx::render
