// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <demo/application.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace demo {

static auto image = sbx::graphics::image_handle{};

application::application()
: sbx::core::application{},
  _is_paused{false},
  _time{0},
  _fps{0} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();

  platform_module.window().on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::quit();
  };

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& resource_registry = graphics_module.resource_registry();

  image = resource_registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = sbx::math::vector3u{1, 1, 1},
    .format = sbx::graphics::format::r32g32b32a32_sfloat,
    .usage = sbx::graphics::image_usage::sampled | sbx::graphics::image_usage::storage,
    .name = std::string{"Test"}
  });
}

auto application::update() -> void {
  using namespace sbx::units::literals;

  if (sbx::platform::input::is_key_pressed(sbx::platform::key::escape)) {
    sbx::core::engine::quit();
  }

  _time += sbx::core::engine::delta_time();

  if (_time >= 1_s) {
    sbx::utility::logger<"demo">::info("FPS: {}", _fps);

    _time -= 1_s;
    _fps = 0;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
 
  auto& frame_context = graphics_module.frame_context();
  
  auto command_buffer = frame_context.begin_frame();
  
  if (!command_buffer) {
    return;
  }
  
  const auto& swapchain = frame_context.swapchain();
  
  const auto image_index = frame_context.active_image_index();
  
  auto to_color_attachment = sbx::graphics::command_buffer::image_transition_data{};
  to_color_attachment.image = swapchain.image(image_index);
  to_color_attachment.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.src_access_mask = VK_ACCESS_2_NONE;
  to_color_attachment.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color_attachment.old_layout = sbx::graphics::image_layout::undefined;
  to_color_attachment.new_layout = sbx::graphics::image_layout::color_attachment_optimal;
  
  command_buffer->transition_image_layout(to_color_attachment);
  
  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain.image_view(image_index);
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{0.7f, 0.2f, 0.2f, 1.0f}};
  
  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, swapchain.extent()};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;
  
  command_buffer->begin_rendering(rendering_info);

  command_buffer->end_rendering();
  
  auto to_present_src = sbx::graphics::command_buffer::image_transition_data{};
  to_present_src.image = swapchain.image(image_index);
  to_present_src.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_present_src.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_present_src.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  to_present_src.dst_access_mask = VK_ACCESS_2_NONE;
  to_present_src.old_layout = sbx::graphics::image_layout::color_attachment_optimal;
  to_present_src.new_layout = sbx::graphics::image_layout::present_source;
  
  command_buffer->transition_image_layout(to_present_src);
  
  frame_context.end_frame();

  ++_fps;
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
