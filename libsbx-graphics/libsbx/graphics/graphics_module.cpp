// SPDX-License-Identifier: MIT
#include <libsbx/graphics/graphics_module.hpp>

#include <ranges>

#include <fmt/format.h>

#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::graphics {

#include <vulkan/vulkan.h>

static auto vk_result_to_string(VkResult result) -> const char* {
  switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";

    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";

    // case VK_ERROR_VALIDATION_FAILED: return "VK_ERROR_VALIDATION_FAILED";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";

    case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";

    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";

    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";

    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";

    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";

    case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";

    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";

    case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
    case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";

    default: return "VK_RESULT_UNKNOWN";
  }
}


auto validate(VkResult result) -> void {
  if (result >= VK_SUCCESS) {
    return;
  }

  throw utility::runtime_error{"Validation error: {}", std::string{vk_result_to_string(result)}};
}

graphics_module::graphics_module()
: _instance{memory::make_unique<graphics::instance>()},
  _physical_device{memory::make_unique<graphics::physical_device>(*_instance)},
  _logical_device{memory::make_unique<graphics::logical_device>(*_physical_device)},
  _surface{memory::make_unique<graphics::surface>(*_instance, *_physical_device, *_logical_device)},
  _allocator{*_instance, *_physical_device, *_logical_device},
  _query_pool{*_logical_device, VK_QUERY_TYPE_TIMESTAMP, 1000},
  _is_framebuffer_resized{true},
  _is_viewport_resized{true} {
  auto& devices_module = core::engine::get_module<devices::devices_module>();

  auto& window = devices_module.window();

  window.on_framebuffer_resized() += [this]([[maybe_unused]] const auto& event) {
    _is_framebuffer_resized = true;
  };

  _dynamic_size_callback = [this]() -> sbx::math::vector2u {
    return math::vector2u{_surface->current_extent().width, _surface->current_extent().height};
  };

  _graphics_command_buffers.reserve(swapchain::max_frames_in_flight);
  _compute_command_buffers.reserve(swapchain::max_frames_in_flight);
}

graphics_module::~graphics_module() {
  _logical_device->wait_idle();

  _renderer.reset();

  _swapchain.reset();

  for (const auto& frame_data : _per_frame_data) {
    vkDestroyFence(*_logical_device, frame_data.graphics_in_flight_fence, nullptr);
    vkDestroyFence(*_logical_device, frame_data.compute_in_flight_fence, nullptr);
    vkDestroySemaphore(*_logical_device, frame_data.image_available_semaphore, nullptr);
    vkDestroySemaphore(*_logical_device, frame_data.compute_finished_semaphore, nullptr);
  }

  for (const auto& image_data : _per_image_data) {
    vkDestroySemaphore(*_logical_device, image_data.render_finished_semaphore, nullptr);
  }

  // [NOTE] KAJ 2023-02-19 : Command buffers must be freed before the command pools
  // _graphics_command_buffers.clear();
  // _compute_command_buffers.clear();
  _command_pools.clear();

  _buffers.clear();
  _storage_buffers.clear();
  _uniform_buffers.clear();
  _shaders.clear();
  _graphics_pipelines.clear();
  _compute_pipelines.clear();
  _images.clear();
  _depth_images.clear();
  _cube_images.clear();
  _sampler_states.clear();
}

auto graphics_module::update() -> void {
  SBX_PROFILE_SCOPE("graphics_module::update");

  auto& devices_module = core::engine::get_module<devices::devices_module>();

  const auto& window = devices_module.window();

  if (!_renderer || window.is_iconified()) {
    return;
  }

  const auto& frame_data = _per_frame_data[_current_frame];

  if (_is_framebuffer_resized || _swapchain->is_outdated(_surface->current_extent())) {
    _recreate_swapchain();
    return;
  }

  auto viewport = std::invoke(_dynamic_size_callback);

  if (viewport != _viewport) {
    _viewport = viewport;
    _recreate_viewport();

    return;
  }

  SBX_PROFILE_BLOCK("graphics_module::acquire_next_image") {
    // Get the next image in the swapchain (back/front buffer)
    EASY_BLOCK("wait for image");
    const auto result = _swapchain->acquire_next_image(frame_data.image_available_semaphore, frame_data.graphics_in_flight_fence);
    EASY_END_BLOCK;
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      _recreate_swapchain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error{"Failed to acquire swapchain image"};
    }
  }

  EASY_BLOCK("draw");

  auto& command_buffer = _graphics_command_buffers[_current_frame];
  
  command_buffer.reset();
  command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

  _query_pool.reset(command_buffer);

  auto& image_data = _per_image_data[_swapchain->active_image_index()];

  _query_pool.write_timestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);
  _renderer->render(command_buffer, *_swapchain);
  _query_pool.write_timestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

  command_buffer.end();

  auto wait_semaphores = std::vector<command_buffer::wait_data>{};
  wait_semaphores.push_back({frame_data.image_available_semaphore, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT});

  command_buffer.submit(wait_semaphores, image_data.render_finished_semaphore, frame_data.graphics_in_flight_fence);

  auto duration = _query_pool.get_duration(0, 1);

  // Present the image to the screen
  const auto result = _swapchain->present(image_data.render_finished_semaphore);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _is_framebuffer_resized) {
    _recreate_swapchain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error{"Failed to present swapchain image"};
  }

  _current_frame = utility::fast_mod(_current_frame + 1, swapchain::max_frames_in_flight);

  EASY_END_BLOCK;
}

auto graphics_module::instance() -> graphics::instance&  {
  return *_instance;
}

auto graphics_module::physical_device() -> graphics::physical_device& {
  return *_physical_device;
}

auto graphics_module::logical_device() -> graphics::logical_device& {
  return *_logical_device;
}

auto graphics_module::surface() -> graphics::surface& {
  return *_surface;
}

auto graphics_module::command_pool(VkQueueFlagBits queue_type, const std::thread::id& thread_id) -> const std::shared_ptr<graphics::command_pool>& {
  const auto key = command_pool_key{queue_type, thread_id};

  if (auto entry = _command_pools.find(key); entry != _command_pools.end()) {
    return entry->second;
  }

  return _command_pools.insert({key, std::make_shared<graphics::command_pool>(queue_type)}).first->second;
}

auto graphics_module::swapchain() -> graphics::swapchain& {
  return *_swapchain;
};

auto graphics_module::attachment(const std::string& name) const -> const descriptor& {
  if (!_renderer) {
    throw std::runtime_error{"No renderer set"};
  }

  return _renderer->attachment(name);
}

auto graphics_module::_recreate_viewport() -> void {
  _logical_device->wait_idle();

  _renderer->resize(viewport::type::dynamic);
  _on_viewport_changed.emit(_viewport);
}

auto graphics_module::_recreate_swapchain() -> void {
  _logical_device->wait_idle();

  _swapchain = memory::make_unique<graphics::swapchain>(_swapchain);

  _recreate_per_frame_data();
  _recreate_per_image_data();
  _recreate_command_buffers();
  _recreate_attachments();

  _viewport = std::invoke(_dynamic_size_callback);

  _renderer->resize(viewport::type::window | viewport::type::dynamic);

  _on_viewport_changed.emit(_viewport);

  _current_frame = 0;
  _is_framebuffer_resized = false;
}

auto graphics_module::_recreate_per_frame_data() -> void {
  for (const auto& data : _per_frame_data) {
    vkDestroyFence(*_logical_device, data.graphics_in_flight_fence, nullptr);
    vkDestroyFence(*_logical_device, data.compute_in_flight_fence, nullptr);
    vkDestroySemaphore(*_logical_device, data.image_available_semaphore, nullptr);
    vkDestroySemaphore(*_logical_device, data.compute_finished_semaphore, nullptr);
  }

  // _per_frame_data.resize(swapchain::max_frames_in_flight);

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto fence_create_info = VkFenceCreateInfo{};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto& data : _per_frame_data) {
    validate(vkCreateSemaphore(*_logical_device, &semaphore_create_info, nullptr, &data.image_available_semaphore));
    validate(vkCreateSemaphore(*_logical_device, &semaphore_create_info, nullptr, &data.compute_finished_semaphore));
    validate(vkCreateFence(*_logical_device, &fence_create_info, nullptr, &data.graphics_in_flight_fence));
    validate(vkCreateFence(*_logical_device, &fence_create_info, nullptr, &data.compute_in_flight_fence));
  }
}

auto graphics_module::_recreate_per_image_data() -> void {
  for (auto& data : _per_image_data) {
    vkDestroySemaphore(*_logical_device, data.render_finished_semaphore, nullptr);
  }

  _per_image_data.resize(_swapchain->image_count());

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto& data : _per_image_data) {
    validate(vkCreateSemaphore(*_logical_device, &semaphore_create_info, nullptr, &data.render_finished_semaphore));
  }
}

auto graphics_module::_recreate_command_buffers() -> void {
  for (auto i = _graphics_command_buffers.size(); i < swapchain::max_frames_in_flight; ++i) {
    _graphics_command_buffers.emplace_back(false, VK_QUEUE_GRAPHICS_BIT);
  }

  for (auto& command_buffer : _graphics_command_buffers) {
    command_buffer.reset();
  }

  for (auto i = _compute_command_buffers.size(); i < swapchain::max_frames_in_flight; ++i) {
    _compute_command_buffers.emplace_back(false, VK_QUEUE_COMPUTE_BIT);
  }

  for (auto& command_buffer : _compute_command_buffers) {
    command_buffer.reset();
  }
}

auto graphics_module::_recreate_attachments() -> void {
  _attachments.clear();
}

} // namespace sbx::graphics
