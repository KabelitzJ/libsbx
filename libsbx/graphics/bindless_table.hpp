// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_BINDLESS_TABLE_HPP_
#define LIBSBX_GRAPHICS_BINDLESS_TABLE_HPP_

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/graphics/resources/sampler.hpp>

namespace sbx::graphics {

/**
 * @brief The single global descriptor set (set 0) and the one pipeline layout every pipeline uses.
 *
 * Three update-after-bind, partially-bound arrays: sampled images, samplers, storage images.
 * A resource's array index is its bindless handle and the uint32 a shader indexes with. Indices are
 * handed out by per-array allocators. Descriptor writes are batched by register_* and applied by
 * flush_writes(), which frame_context calls once per frame after collect and before the passes.
 */
class bindless_table : public utility::noncopyable {

public:

  inline static constexpr auto set_index = std::uint32_t{0u};
  inline static constexpr auto sampled_image_binding = std::uint32_t{0u};
  inline static constexpr auto sampler_binding = std::uint32_t{1u};
  inline static constexpr auto storage_image_binding = std::uint32_t{2u};
  inline static constexpr auto push_constant_size = std::uint32_t{128u};

  bindless_table(const physical_device& physical_device, const logical_device& logical_device);

  ~bindless_table();

  [[nodiscard]] auto descriptor_set_layout() const noexcept -> VkDescriptorSetLayout {
    return _descriptor_set_layout;
  }

  [[nodiscard]] auto descriptor_set() const noexcept -> VkDescriptorSet {
    return _descriptor_set;
  }

  [[nodiscard]] auto pipeline_layout() const noexcept -> VkPipelineLayout {
    return _pipeline_layout;
  }

  auto register_sampled_image(VkImageView view) -> std::uint32_t;

  auto reserve_sampled_image() -> std::uint32_t;

  auto write_sampled_image(std::uint32_t index, VkImageView view) -> void;

  auto register_storage_image(VkImageView view) -> std::uint32_t;

  auto unregister_sampled_image(std::uint32_t index) -> void;

  auto unregister_storage_image(std::uint32_t index) -> void;

  /**
   * @brief Returns the bindless index of a sampler matching @p create_info, creating and caching it on first request.
   */
  auto sampler_index(const sampler::create_info& create_info) -> std::uint32_t;

  /**
   * @brief Applies every pending descriptor write. Called from frame_context::begin_frame.
   */
  auto flush_writes() -> void;

private:

  struct index_allocator {
    std::uint32_t next{0u};
    std::uint32_t capacity{0u};
    std::vector<std::uint32_t> released{};

    auto allocate() -> std::uint32_t;

    auto release(std::uint32_t index) -> void;
  }; // struct index_allocator

  struct pending_write {
    std::uint32_t binding;
    std::uint32_t array_element;
    VkDescriptorType type;
    VkDescriptorImageInfo image_info;
  }; // struct pending_write

  struct sampler_entry {
    sampler::create_info create_info;
    graphics::sampler sampler;
    std::uint32_t index;
  }; // struct sampler_entry

  VkDescriptorSetLayout _descriptor_set_layout{};
  VkDescriptorPool _descriptor_pool{};
  VkDescriptorSet _descriptor_set{};
  VkPipelineLayout _pipeline_layout{};

  std::mutex _mutex{};

  index_allocator _sampled_images{};
  index_allocator _samplers{};
  index_allocator _storage_images{};

  std::vector<pending_write> _pending_writes{};
  std::vector<sampler_entry> _sampler_cache{};

}; // class bindless_table

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_BINDLESS_TABLE_HPP_
