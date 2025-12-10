#ifndef LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_

#include <memory>

#include <vulkan/vulkan.hpp>

#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>

namespace sbx::graphics {

class command_buffer {

public:

  struct wait_data {
    VkSemaphore semaphore;
    VkPipelineStageFlags stage;
  }; // struct wait_data

  struct buffer_barrier_data {
    std::vector<VkBuffer> buffers;
    VkPipelineStageFlags src_stage_mask;
    VkPipelineStageFlags dst_stage_mask;
    VkAccessFlags src_access_mask;
    VkAccessFlags dst_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
  }; // struct buffer_barrier

  struct release_ownership_data {
    VkPipelineStageFlags2 src_stage_mask;
    VkAccessFlags2 src_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    VkBuffer buffer;
    VkDeviceSize size{VK_WHOLE_SIZE};
    VkDeviceSize offset{0};
  }; // struct release_ownership_data

  struct acquire_ownership_data {
    VkPipelineStageFlags2 dst_stage_mask;
    VkAccessFlags2 dst_access_mask;
    std::uint32_t src_queue_family;
    std::uint32_t dst_queue_family;
    VkBuffer buffer;
    VkDeviceSize size{VK_WHOLE_SIZE};
    VkDeviceSize offset{0};
  }; // struct acquire_ownership_data

  command_buffer(bool should_begin = true, VkQueueFlagBits queue_type = VK_QUEUE_GRAPHICS_BIT, VkCommandBufferLevel buffer_level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  command_buffer(const command_buffer&) = delete;

  command_buffer(command_buffer&&) noexcept;

  ~command_buffer();

  auto operator=(const command_buffer&) -> command_buffer& = delete;

  auto operator=(command_buffer&&) noexcept -> command_buffer&;

  auto handle() const noexcept -> VkCommandBuffer;

  operator VkCommandBuffer() const noexcept;

  auto is_running() const noexcept -> bool;

  auto begin(VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) -> void;

  auto end() -> void;

  auto submit_idle() -> void;

  auto submit(const std::vector<wait_data>& wait_data = {}, const VkSemaphore &signal_semaphore = nullptr, const VkFence& fence = nullptr) -> void;

  auto copy_buffer(const VkBuffer& source, const VkBuffer& destination, const VkBufferCopy& region) -> void;

  auto buffer_barrier(const buffer_barrier_data& buffer_barrier_data) -> void;

  auto memory_dependency(const VkMemoryBarrier2& memory_barrier) -> void;

  auto release_ownership(const std::vector<release_ownership_data>& releases) -> void;

  auto acquire_ownership(const std::vector<acquire_ownership_data>& acquires) -> void;

  auto set_viewport(const VkViewport& viewport) -> void;

  auto set_scissor(const VkRect2D& scissor) -> void;

  auto bind_vertex_buffer(std::uint32_t first_binding, const VkBuffer& buffer) -> void;

  auto bind_index_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkIndexType index_type) -> void;

  auto draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) -> void;

  auto draw_indexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) -> void;

  auto draw_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void;

  auto draw_indexed_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void;

  auto begin_render_pass(const VkRenderPassBeginInfo& renderpass_begin_info, VkSubpassContents subpass_contents) -> void;

  auto end_render_pass() -> void;

  auto fill_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize size, std::uint32_t data) -> void;

  auto begin_rendering(const VkRenderingInfo& rendering_info) -> void;

  auto end_rendering() -> void;

  auto reset(VkCommandBufferResetFlags flags = 0) -> void;

private:

  auto _queue() const -> const graphics::queue&;

  std::shared_ptr<command_pool> _command_pool{};

  VkCommandBuffer _handle{};
  VkQueueFlagBits _queue_type{};
  bool _is_running{};

}; // class command_buffer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_COMMAND_BUFFER_HPP_
