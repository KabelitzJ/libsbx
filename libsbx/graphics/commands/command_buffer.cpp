// SPDX-License-Identifier: MIT
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <limits>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

inline static constexpr auto submit_pipeline_stages = VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

command_buffer::command_buffer(bool should_begin, VkQueueFlagBits queue_type, VkCommandBufferLevel buffer_level)
: _queue_type{queue_type},
  _is_running{false} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  _command_pool = graphics_module.command_pool(queue_type);

  auto command_buffer_allocate_info = VkCommandBufferAllocateInfo{};
	command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_allocate_info.commandPool = *_command_pool;
	command_buffer_allocate_info.level = buffer_level;
	command_buffer_allocate_info.commandBufferCount = 1;

  validate(vkAllocateCommandBuffers(logical_device, &command_buffer_allocate_info, &_handle));

  if (should_begin) {
    begin();
  }
}

command_buffer::command_buffer(command_buffer&& other) noexcept
: _command_pool{std::move(other._command_pool)},
  _handle{std::exchange(other._handle, nullptr)},
  _queue_type{other._queue_type},
  _is_running{std::exchange(other._is_running, false)} { }

command_buffer::~command_buffer() {
  if (!_command_pool || !_handle) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  vkFreeCommandBuffers(logical_device, *_command_pool, 1, &_handle);
}

auto command_buffer::operator=(command_buffer&& other) noexcept -> command_buffer& {
  if (this == &other) {
    return *this;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  if (_is_running) {
		end();
  }

  vkFreeCommandBuffers(logical_device, *_command_pool, 1, &_handle);

  _command_pool = std::move(other._command_pool);
  _handle = std::exchange(other._handle, nullptr);
  _queue_type = other._queue_type;
  _is_running = std::exchange(other._is_running, false);

  return *this;
}

auto command_buffer::handle() const noexcept -> VkCommandBuffer {
  return _handle;
}

command_buffer::operator VkCommandBuffer() const noexcept {
  return _handle;
}

auto command_buffer::is_running() const noexcept -> bool {
  return _is_running;
}

auto command_buffer::begin(VkCommandBufferUsageFlags usage) -> void {
  if (_is_running) {
    utility::logger<"graphics">::warn("Tried to begin recording a command buffer that was already beeing recorded");
    return;
  }

  auto command_buffer_begin_info = VkCommandBufferBeginInfo{};
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.flags = usage;

	validate(vkBeginCommandBuffer(_handle, &command_buffer_begin_info));

	_is_running = true;
}

auto command_buffer::end() -> void {
  if (!_is_running) {
    utility::logger<"graphics">::warn("Tried to stop recording a command buffer that was not beeing recorded");
    return;
  }

  validate(vkEndCommandBuffer(_handle));

  _is_running = false;
}

auto command_buffer::submit_idle() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
	const auto& selected_queue = _queue();

	if (_is_running) {
		end();
  }

	auto submit_info = VkSubmitInfo{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_handle;

	auto fence_create_info = VkFenceCreateInfo{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	auto fence = VkFence{};

	validate(vkCreateFence(logical_device, &fence_create_info, nullptr, &fence));

  validate(vkResetFences(logical_device, 1, &fence));

	validate(vkQueueSubmit(selected_queue, 1, &submit_info, fence));

	validate(vkWaitForFences(logical_device, 1, &fence, true, std::numeric_limits<std::uint64_t>::max()));

	vkDestroyFence(logical_device, fence, nullptr);
}

auto command_buffer::submit(const std::vector<wait_data>& wait_data, const VkSemaphore& signal_semaphore, const VkFence& fence) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
	const auto& selected_queue = _queue();

	if (_is_running) {
		end();
  }

  auto submit_info = VkSubmitInfo{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_handle;

  auto wait_stages = std::vector<VkPipelineStageFlags>{};
  auto wait_semaphores = std::vector<VkSemaphore>{};

  if (!wait_data.empty()) {
    wait_semaphores.reserve(wait_data.size());
    wait_stages.reserve(wait_data.size());

    for (const auto& data : wait_data) {
      wait_semaphores.push_back(data.semaphore);
      wait_stages.push_back(data.stage);
    }

		submit_info.waitSemaphoreCount = static_cast<std::uint32_t>(wait_data.size());
		submit_info.pWaitSemaphores = wait_semaphores.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
	}

	if (signal_semaphore) {
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &signal_semaphore;
	}

	if (fence) {
		validate(vkResetFences(logical_device, 1, &fence));
  }

	validate(vkQueueSubmit(selected_queue, 1, &submit_info, fence));
}

auto command_buffer::copy_buffer(const VkBuffer& source, const VkBuffer& destination, const VkBufferCopy& region) -> void {
  vkCmdCopyBuffer(_handle, source, destination, 1, &region);
}

auto command_buffer::buffer_barrier(const buffer_barrier_data& data) -> void {
  auto buffer_barrier_info = std::vector<VkBufferMemoryBarrier>{};
  buffer_barrier_info.reserve(data.buffers.size());

  for (const auto& buffer : data.buffers) {
    auto buffer_barrier = VkBufferMemoryBarrier{};
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.srcAccessMask = data.src_access_mask;
    buffer_barrier.dstAccessMask = data.dst_access_mask;
    buffer_barrier.srcQueueFamilyIndex = data.src_queue_family;
    buffer_barrier.dstQueueFamilyIndex = data.dst_queue_family;
    buffer_barrier.buffer = buffer;
    buffer_barrier.offset = 0u;
    buffer_barrier.size = VK_WHOLE_SIZE;

    buffer_barrier_info.push_back(buffer_barrier);
  }

  vkCmdPipelineBarrier(_handle, data.src_stage_mask, data.dst_stage_mask, 0, 0, nullptr, static_cast<std::uint32_t>(buffer_barrier_info.size()), buffer_barrier_info.data(), 0, nullptr);
}

auto command_buffer::acquire_image_ownership(const std::vector<image_acquire_data>& acquires) -> void {
  if (acquires.empty()) {
    return;
  }

  auto barriers = std::vector<VkImageMemoryBarrier2>{};
  barriers.reserve(acquires.size());

  for (const auto& data : acquires) {
    auto image_barrier = VkImageMemoryBarrier2{};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    image_barrier.srcAccessMask = 0;
    image_barrier.dstStageMask = data.dst_stage_mask;
    image_barrier.dstAccessMask = data.dst_access_mask;
    image_barrier.oldLayout = data.old_layout;
    image_barrier.newLayout = data.new_layout;
    image_barrier.srcQueueFamilyIndex = data.src_queue_family;
    image_barrier.dstQueueFamilyIndex = data.dst_queue_family;
    image_barrier.image = data.image;
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0u;
    image_barrier.subresourceRange.levelCount = data.mip_levels;
    image_barrier.subresourceRange.baseArrayLayer = data.base_array_layer;
    image_barrier.subresourceRange.layerCount = data.layer_count;

    barriers.push_back(image_barrier);
  }

  auto dependency_info = VkDependencyInfo{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
  dependency_info.pImageMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(_handle, &dependency_info);
}

auto command_buffer::release_image_ownership(const std::vector<image_release_data>& releases) -> void {
  if (releases.empty()) {
    return;
  }

  auto barriers = std::vector<VkImageMemoryBarrier2>{};
  barriers.reserve(releases.size());

  for (const auto& data : releases) {
    auto image_barrier = VkImageMemoryBarrier2{};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_barrier.srcStageMask = data.src_stage_mask;
    image_barrier.srcAccessMask = data.src_access_mask;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    image_barrier.dstAccessMask = 0;
    image_barrier.oldLayout = data.old_layout;
    image_barrier.newLayout = data.new_layout;
    image_barrier.srcQueueFamilyIndex = data.src_queue_family;
    image_barrier.dstQueueFamilyIndex = data.dst_queue_family;
    image_barrier.image = data.image;
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0u;
    image_barrier.subresourceRange.levelCount = data.mip_levels;
    image_barrier.subresourceRange.baseArrayLayer = data.base_array_layer;
    image_barrier.subresourceRange.layerCount = data.layer_count;

    barriers.push_back(image_barrier);
  }

  auto dependency_info = VkDependencyInfo{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
  dependency_info.pImageMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(_handle, &dependency_info);
}

auto command_buffer::memory_dependency(const VkMemoryBarrier2& memory_barrier) -> void {
  auto dependency_info = VkDependencyInfo{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.memoryBarrierCount = 1u;
  dependency_info.pMemoryBarriers = &memory_barrier;

  vkCmdPipelineBarrier2(_handle, &dependency_info);
}

auto command_buffer::release_buffer_ownership(const std::vector<buffer_release_data>& releases) -> void {
  if (releases.empty()) {
    return;
  }

  auto barriers = std::vector<VkBufferMemoryBarrier2>{};
  barriers.reserve(releases.size());

  for (const auto& data : releases) {
    auto buffer_barrier = VkBufferMemoryBarrier2{ };
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    buffer_barrier.srcStageMask = data.src_stage_mask;
    buffer_barrier.srcAccessMask = data.src_access_mask;
    buffer_barrier.srcQueueFamilyIndex = data.src_queue_family;
    buffer_barrier.dstQueueFamilyIndex = data.dst_queue_family;
    buffer_barrier.buffer = data.buffer;
    buffer_barrier.offset = data.offset;
    buffer_barrier.size = data.size;

    barriers.push_back(buffer_barrier);
  }

  auto dependency_info = VkDependencyInfo{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
  dependency_info.pBufferMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(_handle, &dependency_info);
}

auto command_buffer::acquire_buffer_ownership(const std::vector<buffer_acquire_data>& acquires) -> void {
  if (acquires.empty()) {
    return;
  }

  auto barriers = std::vector<VkBufferMemoryBarrier2>{};
  barriers.reserve(acquires.size());

  for (const auto& data : acquires) {
    auto buffer_barrier = VkBufferMemoryBarrier2{};
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    buffer_barrier.dstStageMask = data.dst_stage_mask;
    buffer_barrier.dstAccessMask = data.dst_access_mask;
    buffer_barrier.srcQueueFamilyIndex = data.src_queue_family;
    buffer_barrier.dstQueueFamilyIndex = data.dst_queue_family;
    buffer_barrier.buffer = data.buffer;
    buffer_barrier.offset = data.offset;
    buffer_barrier.size = data.size;

    barriers.push_back(buffer_barrier);
  }

  auto dependency_info = VkDependencyInfo{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
  dependency_info.pBufferMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(_handle, &dependency_info);
}

auto command_buffer::set_viewport(const VkViewport& viewport) -> void {
  vkCmdSetViewport(_handle, 0, 1, &viewport);
}

auto command_buffer::set_scissor(const VkRect2D& scissor) -> void {
  vkCmdSetScissor(_handle, 0, 1, &scissor); 
}

// auto command_buffer::bind_pipeline(const pipeline& pipeline, VkPipelineBindPoint bind_point) -> void {
//   vkCmdBindPipeline(_handle, bind_point, pipeline);
// }

auto command_buffer::bind_vertex_buffer(std::uint32_t first_binding, const VkBuffer& buffer) -> void {
  auto buffers = std::array<VkBuffer, 1>{buffer};
  auto offsets = std::array<VkDeviceSize, 1>{0};

  vkCmdBindVertexBuffers(_handle, first_binding, 1, buffers.data(), offsets.data());
}

auto command_buffer::bind_index_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkIndexType index_type) -> void {
  vkCmdBindIndexBuffer(_handle, buffer, offset, index_type);
}

auto command_buffer::draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) -> void {
  vkCmdDraw(_handle, vertex_count, instance_count, first_vertex, first_instance);
}

auto command_buffer::draw_indexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) -> void {
  vkCmdDrawIndexed(_handle, index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto command_buffer::draw_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void {
  vkCmdDrawIndirect(_handle, buffer, offset * sizeof(VkDrawIndirectCommand), count, sizeof(VkDrawIndirectCommand));
}

auto command_buffer::draw_indexed_indirect(VkBuffer buffer, std::uint32_t offset, std::uint32_t count) -> void {
  vkCmdDrawIndexedIndirect(_handle, buffer, offset * sizeof(VkDrawIndexedIndirectCommand), count, sizeof(VkDrawIndexedIndirectCommand));
}

auto command_buffer::draw_indexed_indirect_count(VkBuffer buffer, std::uint32_t offset, VkBuffer count_buffer, std::uint32_t count_buffer_offset, std::uint32_t max_draw_count) -> void {
  vkCmdDrawIndexedIndirectCount(_handle, buffer, offset * sizeof(VkDrawIndexedIndirectCommand), count_buffer, count_buffer_offset * sizeof(std::uint32_t), max_draw_count, sizeof(VkDrawIndexedIndirectCommand));
}

auto command_buffer::begin_render_pass(const VkRenderPassBeginInfo& renderpass_begin_info, VkSubpassContents subpass_contents) -> void {
  vkCmdBeginRenderPass(_handle, &renderpass_begin_info, subpass_contents);
}

auto command_buffer::end_render_pass() -> void {
  vkCmdEndRenderPass(_handle);
}

auto command_buffer::fill_buffer(const VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize size, std::uint32_t data) -> void {
  vkCmdFillBuffer(_handle, buffer, offset, size, data);
}

auto command_buffer::begin_rendering(const VkRenderingInfo& rendering_info) -> void {
  vkCmdBeginRendering(_handle, &rendering_info);
}

auto command_buffer::end_rendering() -> void {
  vkCmdEndRendering(_handle);
}

auto command_buffer::reset(VkCommandBufferResetFlags flags) -> void {
  validate(vkResetCommandBuffer(_handle, flags));
  _is_running = false;
}

auto command_buffer::execute_commands(const std::vector<command_buffer>& commands) -> void {
  auto handles = std::vector<VkCommandBuffer>{};
  handles.reserve(commands.size());

  for (const auto& command : commands) {
    handles.push_back(command);
  }

  vkCmdExecuteCommands(_handle, handles.size(), handles.data());
}

auto command_buffer::_queue() const -> const graphics::queue& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  switch (_queue_type) {
    case VK_QUEUE_GRAPHICS_BIT: {
      return logical_device.queue<graphics::queue::type::graphics>();
    }
    case VK_QUEUE_COMPUTE_BIT: {
      return logical_device.queue<graphics::queue::type::compute>();
    }
    case VK_QUEUE_TRANSFER_BIT: {
      return logical_device.queue<graphics::queue::type::transfer>();
    }
    default: {
      throw std::runtime_error{"Invalid queue type"};
    }
  }
}

} // namespace sbx::graphics
