// SPDX-License-Identifier: MIT
#include <libsbx/graphics/render_graph.hpp>

#include <libsbx/utility/logger.hpp>

namespace sbx::graphics {

template<typename... Args>
requires (std::is_constructible_v<attachment, Args...>)
auto render_graph::create_attachment(Args&&... args) -> attachment_handle {
  auto index = static_cast<std::uint32_t>(_attachments.size());

  _attachments.emplace_back(std::forward<Args>(args)...);
  _image_by_name.emplace(_attachments.back().name(), index);

  return attachment_handle{index};
}

template<typename Callable>
requires (std::is_invocable_r_v<pass_node, Callable, render_graph::context&>)
auto render_graph::create_pass(Callable&& callable) -> pass_handle {
  auto index = static_cast<std::uint32_t>(_passes.size());

  auto ctx = context{};

  _passes.emplace_back(std::invoke(callable, ctx));

  return pass_handle{index};
}

template<typename Callable>
auto render_graph::_execute_pass_instruction(command_buffer& command_buffer, const swapchain& swapchain, const pass_instruction& instruction, Callable&& callable) -> void {
  const auto& pass = _passes[instruction.pass.index];
  const auto& area = pass._render_area;

  auto render_area = VkRect2D{};
  render_area.offset = VkOffset2D{static_cast<std::int32_t>(area.offset().x()), static_cast<std::int32_t>(area.offset().y())};
  render_area.extent = VkExtent2D{static_cast<std::uint32_t>(area.extent().x()), static_cast<std::uint32_t>(area.extent().y())};

  auto viewport = VkViewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(render_area.extent.width);
  viewport.height = static_cast<float>(render_area.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  command_buffer.set_viewport(viewport);
  command_buffer.set_scissor(render_area);

  auto color_attachments = std::vector<VkRenderingAttachmentInfo>{};
  color_attachments.reserve(instruction.attachments.size());

  auto depth_attachment = std::optional<VkRenderingAttachmentInfo>{};

  auto max_array_layers = std::uint32_t{1u};

  for (const auto& [handle, load_op] : instruction.attachments) {
    auto& state = _attachment_states[handle.index];
    const auto& attachment = _attachments[handle.index];

    max_array_layers = std::max(max_array_layers, state.array_layers);

    if (state.type == attachment::type::image || state.type == attachment::type::swapchain) {
      color_attachments.emplace_back(_build_color_attachment_info(attachment, state, swapchain, load_op));
    } else if (state.type == attachment::type::depth) {
      depth_attachment = _build_depth_attachment_info(attachment, state, load_op);
    }
  }

  const auto view_mask = (max_array_layers > 1u) ? ((1u << max_array_layers) - 1u) : 0u;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = render_area;
  rendering_info.layerCount = 1;
  rendering_info.viewMask = view_mask;
  rendering_info.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size());
  rendering_info.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data();
  rendering_info.pDepthAttachment = depth_attachment ? &*depth_attachment : nullptr;
  rendering_info.pStencilAttachment = depth_attachment ? &*depth_attachment : nullptr;

  command_buffer.begin_rendering(rendering_info);

  std::invoke(std::forward<Callable>(callable), instruction.pass);

  command_buffer.end_rendering();
}

template<typename Callable>
auto render_graph::_execute_compute_instruction(command_buffer& command_buffer, const compute_instruction& instruction, Callable&& callable) -> void {
  (void)command_buffer;

  std::invoke(std::forward<Callable>(callable), instruction.pass);
}

} // namespace sbx::graphics
