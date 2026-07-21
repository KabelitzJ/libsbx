// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/fast_mod.hpp>

#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

static auto _device_type_name(const VkPhysicalDeviceType type) -> std::string_view {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
  }
}

graphics_module::graphics_module()
: _instance{},
  _physical_device{_instance},
  _logical_device{_physical_device},
  _allocator{_instance, _physical_device, _logical_device},
  _surface{_instance, _physical_device, _logical_device},
  _is_framebuffer_resized{true},
  _current_frame{0} {
  const auto& properties = _physical_device.properties();

  utility::logger<"graphics">::info("Device: {} ({})", std::string_view{properties.deviceName}, _device_type_name(properties.deviceType));
  utility::logger<"graphics">::info("Api version: {}.{}.{}", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
  utility::logger<"graphics">::info("Device local memory: {} MiB", _physical_device.device_local_memory() / (1024u * 1024u));
  utility::logger<"graphics">::info("Queue families: ");
  utility::logger<"graphics">::info("  Graphics: {}", _logical_device.queue<queue::type::graphics>().family());
  utility::logger<"graphics">::info("  Present: {}", _logical_device.queue<queue::type::present>().family());
  utility::logger<"graphics">::info("  Compute: {}", _logical_device.queue<queue::type::compute>().family());
  utility::logger<"graphics">::info("  Transfer: {}", _logical_device.queue<queue::type::transfer>().family());

  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  window.on_framebuffer_resized() += [this]([[maybe_unused]] const platform::framebuffer_resized_event& event) {
    _is_framebuffer_resized = true;
  };
}

graphics_module::~graphics_module() {
  _logical_device.wait_idle();

  _swapchain.reset();

  for (const auto& frame_data : _per_frame_data) {
    vkDestroyFence(_logical_device, frame_data.graphics_in_flight_fence, nullptr);
    vkDestroyFence(_logical_device, frame_data.compute_in_flight_fence, nullptr);
    vkDestroySemaphore(_logical_device, frame_data.image_available_semaphore, nullptr);
    vkDestroySemaphore(_logical_device, frame_data.compute_finished_semaphore, nullptr);
  }

  for (const auto& image_data : _per_image_data) {
    vkDestroySemaphore(_logical_device, image_data.render_finished_semaphore, nullptr);
  }

  _graphics_command_buffers.clear();
  _compute_command_buffers.clear();
  _command_pools.clear();
}

auto graphics_module::update() -> void {
  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  if (window.is_iconified()) {
    return;
  }

  auto& frame_data = _per_frame_data[_current_frame];

  if (_is_framebuffer_resized || !_swapchain || _swapchain->is_outdated(_surface.current_extent())) {
    _recreate_swapchain();
    return;
  }

  const auto acquire_result = _swapchain->acquire_next_image(frame_data.image_available_semaphore, frame_data.graphics_in_flight_fence);
  
  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    _recreate_swapchain();
    return;
  } else if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error{"Failed to acquire swapchain image"};
  }

  auto& command_buffer = _graphics_command_buffers[_current_frame];
  
  command_buffer.reset();
  command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  const auto image_index = _swapchain->active_image_index();

  auto& image_data = _per_image_data[image_index];

  auto to_color_attachment = command_buffer::image_transition_data{};
  to_color_attachment.image = _swapchain->image(image_index);
  to_color_attachment.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.src_access_mask = VK_ACCESS_2_NONE;
  to_color_attachment.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color_attachment.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  to_color_attachment.new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  command_buffer.transition_image_layout(to_color_attachment);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = _swapchain->image_view(image_index);
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{0.1f, 0.2f, 0.7f, 1.0f}};

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, _swapchain->extent()};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;

  command_buffer.begin_rendering(rendering_info);
  command_buffer.end_rendering();

  auto to_present_src = command_buffer::image_transition_data{};
  to_present_src.image = _swapchain->image(image_index);
  to_present_src.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_present_src.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_present_src.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  to_present_src.dst_access_mask = VK_ACCESS_2_NONE;
  to_present_src.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  to_present_src.new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  command_buffer.transition_image_layout(to_present_src);

  command_buffer.end();

  auto wait_semaphores = std::vector<command_buffer::wait_semaphore>{};
  wait_semaphores.push_back({frame_data.image_available_semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});

  command_buffer.submit(wait_semaphores, image_data.render_finished_semaphore, frame_data.graphics_in_flight_fence);

  // Present the image to the screen
  const auto present_result = _swapchain->present(image_data.render_finished_semaphore);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || _is_framebuffer_resized) {
    _recreate_swapchain();
  } else if (present_result != VK_SUCCESS) {
    throw std::runtime_error{"Failed to present swapchain image"};
  }

  _current_frame = utility::fast_mod(_current_frame + 1, swapchain::max_frames_in_flight);
}

auto graphics_module::command_pool(const queue::type type, const std::thread::id& thread_id) -> const std::shared_ptr<graphics::command_pool>& {
  const auto key = command_pool_key{type, thread_id};

  if (auto entry = _command_pools.find(key); entry != _command_pools.end()) {
    return entry->second;
  }

  return _command_pools.insert({key, std::make_shared<graphics::command_pool>(type)}).first->second;
}

auto graphics_module::_recreate_swapchain() -> void {
  _logical_device.wait_idle();

  _swapchain = std::make_unique<graphics::swapchain>(_swapchain);

  _recreate_per_frame_data();
  _recreate_per_image_data();
  _recreate_command_buffers();

  _is_framebuffer_resized = false;
  _current_frame = 0;
}

auto graphics_module::_recreate_per_frame_data() -> void {
  for (const auto& data : _per_frame_data) {
    vkDestroyFence(_logical_device, data.graphics_in_flight_fence, nullptr);
    vkDestroyFence(_logical_device, data.compute_in_flight_fence, nullptr);
    vkDestroySemaphore(_logical_device, data.image_available_semaphore, nullptr);
    vkDestroySemaphore(_logical_device, data.compute_finished_semaphore, nullptr);
  }

  // _per_frame_data.resize(swapchain::max_frames_in_flight);

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto fence_create_info = VkFenceCreateInfo{};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto& data : _per_frame_data) {
    validate(vkCreateSemaphore(_logical_device, &semaphore_create_info, nullptr, &data.image_available_semaphore), "vkCreateSemaphore(image_available_semaphore)");
    validate(vkCreateSemaphore(_logical_device, &semaphore_create_info, nullptr, &data.compute_finished_semaphore), "vkCreateSemaphore(compute_finished_semaphore)");
    validate(vkCreateFence(_logical_device, &fence_create_info, nullptr, &data.graphics_in_flight_fence), "vkCreateFence(graphics_in_flight_fence)");
    validate(vkCreateFence(_logical_device, &fence_create_info, nullptr, &data.compute_in_flight_fence), "vkCreateFence(compute_in_flight_fence)");

    _logical_device.set_debug_name(data.image_available_semaphore, "image_available_semaphore");
    _logical_device.set_debug_name(data.compute_finished_semaphore, "compute_finished_semaphore");
    _logical_device.set_debug_name(data.graphics_in_flight_fence, "graphics_in_flight_fence");
    _logical_device.set_debug_name(data.compute_in_flight_fence, "compute_in_flight_fence");
  }
}

auto graphics_module::_recreate_per_image_data() -> void {
  for (auto& data : _per_image_data) {
    vkDestroySemaphore(_logical_device, data.render_finished_semaphore, nullptr);
  }

  _per_image_data.resize(_swapchain->image_count());

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto& data : _per_image_data) {
    validate(vkCreateSemaphore(_logical_device, &semaphore_create_info, nullptr, &data.render_finished_semaphore), "vkCreateSemaphore(render_finished_semaphore)");

    _logical_device.set_debug_name(data.render_finished_semaphore, "render_finished_semaphore");
  }
}

auto graphics_module::_recreate_command_buffers() -> void {
  for (auto i = _graphics_command_buffers.size(); i < swapchain::max_frames_in_flight; ++i) {
    _graphics_command_buffers.emplace_back(queue::type::graphics, false);
  }

  for (auto& command_buffer : _graphics_command_buffers) {
    command_buffer.reset();
  }

  for (auto i = _compute_command_buffers.size(); i < swapchain::max_frames_in_flight; ++i) {
    _compute_command_buffers.emplace_back(queue::type::compute, false);
  }

  for (auto& command_buffer : _compute_command_buffers) {
    command_buffer.reset();
  }
}

} // namespace sbx::graphics
