// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_

#include <memory>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>

#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/graphics/bindless_table.hpp>
#include <libsbx/graphics/types.hpp>

namespace sbx::graphics {

class command_buffer : public utility::noncopyable {

public:

  struct wait_semaphore {
    VkSemaphore semaphore;
    VkPipelineStageFlags stage;
  }; // struct wait_semaphore

  struct buffer_barrier_data {
    std::vector<VkBuffer> buffers;
    VkPipelineStageFlags src_stage_mask;
    VkPipelineStageFlags dst_stage_mask;
    VkAccessFlags src_access_mask;
    VkAccessFlags dst_access_mask;
    std::uint32_t src_queue_family{VK_QUEUE_FAMILY_IGNORED};
    std::uint32_t dst_queue_family{VK_QUEUE_FAMILY_IGNORED};
  }; // struct buffer_barrier

  struct buffer_acquire_data {
    VkPipelineStageFlags2 dst_stage_mask;
    VkAccessFlags2 dst_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    VkBuffer buffer;
    VkDeviceSize size{VK_WHOLE_SIZE};
    VkDeviceSize offset{0};
  }; // struct buffer_acquire_data

  struct buffer_release_data {
    VkPipelineStageFlags2 src_stage_mask;
    VkAccessFlags2 src_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    VkBuffer buffer;
    VkDeviceSize size{VK_WHOLE_SIZE};
    VkDeviceSize offset{0};
  }; // struct buffer_release_data

  struct image_release_data {
    VkImage image;
    std::uint32_t mip_levels;
    std::uint32_t base_array_layer;
    std::uint32_t layer_count;
    VkPipelineStageFlags2 src_stage_mask;
    VkAccessFlags2 src_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    graphics::image_layout old_layout;
    graphics::image_layout new_layout;
  }; // struct image_release_data

  struct image_acquire_data {
    VkImage image;
    std::uint32_t mip_levels;
    std::uint32_t base_array_layer;
    std::uint32_t layer_count;
    VkPipelineStageFlags2 dst_stage_mask;
    VkAccessFlags2 dst_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    graphics::image_layout old_layout;
    graphics::image_layout new_layout;
  }; // struct image_acquire_data

  struct image_transition_data {
    graphics::image::handle_type image;
    VkPipelineStageFlags2 src_stage_mask;
    VkAccessFlags2 src_access_mask;
    VkPipelineStageFlags2 dst_stage_mask;
    VkAccessFlags2 dst_access_mask;
    graphics::image_layout old_layout;
    graphics::image_layout new_layout;
    VkImageAspectFlags aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
    std::uint32_t base_mip_level{0u};
    std::uint32_t mip_levels{1u};
    std::uint32_t base_array_layer{0u};
    std::uint32_t layer_count{1u};
  }; // struct image_transition_data

  command_buffer(const queue::type type, bool should_begin = true, VkCommandBufferLevel buffer_level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  command_buffer(command_buffer&&) noexcept;

  ~command_buffer();

  auto operator=(command_buffer&&) noexcept -> command_buffer&;

  auto handle() const noexcept -> VkCommandBuffer;

  operator VkCommandBuffer() const noexcept;

  auto type() const noexcept -> queue::type;

  auto is_running() const noexcept -> bool;

  auto begin(VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) -> void;

  auto end() -> void;

  auto submit_idle() -> void;

  auto submit(const std::vector<wait_semaphore>& wait_semaphores = {}, const VkSemaphore& signal_semaphore = nullptr, const VkFence& fence = nullptr) -> void;

  auto copy_buffer(const VkBuffer& source, const VkBuffer& destination, const VkBufferCopy& region) -> void;

  auto buffer_barrier(const buffer_barrier_data& buffer_barrier_data) -> void;

  auto memory_dependency(const VkMemoryBarrier2& memory_barrier) -> void;

  auto release_buffer_ownership(const std::vector<buffer_release_data>& releases) -> void;

  auto acquire_buffer_ownership(const std::vector<buffer_acquire_data>& acquires) -> void;

  auto acquire_image_ownership(const std::vector<image_acquire_data>& acquires) -> void;

  auto transition_image_layout(const image_transition_data& transition_data) -> void;

  auto release_image_ownership(const std::vector<image_release_data>& releases) -> void;

  auto set_viewport(const VkViewport& viewport) -> void;

  auto set_scissor(const VkRect2D& scissor) -> void;

  auto bind_vertex_buffer(std::uint32_t first_binding, const VkBuffer& buffer) -> void;

  auto bind_index_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkIndexType index_type) -> void;

  auto bind_pipeline(const graphics::graphics_pipeline& pipeline) -> void;

  auto bind_pipeline(const graphics::compute_pipeline& pipeline) -> void;

  auto dispatch(std::uint32_t group_count_x, std::uint32_t group_count_y, std::uint32_t group_count_z) -> void;

  auto dispatch_indirect(VkBuffer buffer, VkDeviceSize offset = 0u) -> void;

  auto draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) -> void;

  auto draw_indexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) -> void;

  auto draw_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void;

  auto draw_indexed_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void;

  auto draw_indexed_indirect_count(VkBuffer buffer, std::uint32_t offset, VkBuffer count_buffer, std::uint32_t count_buffer_offset, std::uint32_t max_draw_count) -> void;

  auto begin_render_pass(const VkRenderPassBeginInfo& renderpass_begin_info, VkSubpassContents subpass_contents) -> void;

  auto end_render_pass() -> void;

  auto fill_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize size, std::uint32_t data) -> void;

  auto push_constants(const VkPipelineLayout& layout, VkShaderStageFlags stage_flags, std::uint32_t offset, const std::array<std::byte, graphics::bindless_table::push_constant_size>& values) -> void;

  auto begin_rendering(const VkRenderingInfo& rendering_info) -> void;

  auto end_rendering() -> void;

  auto reset(VkCommandBufferResetFlags flags = 0) -> void;

  auto execute_commands(const std::vector<command_buffer>& commands) -> void;

private:

  std::shared_ptr<command_pool> _command_pool;

  VkCommandBuffer _handle;
  queue::type _queue_type;
  bool _is_recording;

}; // class command_buffer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_
