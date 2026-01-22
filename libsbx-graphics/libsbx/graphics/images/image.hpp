// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_IMAGES_IMAGE_HPP_
#define LIBSBX_GRAPHICS_IMAGES_IMAGE_HPP_

#include <cinttypes>

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

namespace sbx::graphics {

class image : public utility::noncopyable, public descriptor {

public:

  image(const VkExtent3D extent, VkFilter filter, VkSamplerAddressMode address_mode, VkSampleCountFlagBits samples, VkImageLayout layout, VkImageUsageFlags usage, VkFormat format, std::uint32_t mip_levels, std::uint32_t array_layers);

  virtual ~image();

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags, std::uint32_t count = 1u) noexcept -> VkDescriptorSetLayoutBinding;

  static auto mip_levels(const VkExtent3D& extent) noexcept -> std::uint32_t;

  static auto find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) noexcept -> VkFormat;

  static auto has_depth_component(VkFormat format) noexcept -> bool;

  static auto has_stencil_component(VkFormat format) noexcept -> bool;

  static auto create_image(VkImage& image, VmaAllocation& allocation, const VkExtent3D& extent, VkFormat format, VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::uint32_t mip_levels, std::uint32_t array_layers, VkImageType type) -> void;

  static auto create_image_view(const VkImage& image, VkImageView& image_view, VkImageViewType type, VkFormat format, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void;

  static auto create_image_sampler(VkSampler& sampler, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, std::uint32_t mip_levels) -> void;

  static auto create_mipmaps(const VkImage& image, const VkExtent3D& extent, VkFormat format, VkImageLayout dst_image_layout, std::uint32_t mip_levels, std::uint32_t base_array_layer, std::uint32_t layer_count) -> void;

  static auto transition_image_layout(const VkImage& image, VkFormat format, VkImageLayout src_image_layout, VkImageLayout dst_image_layout, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void;

  static auto transition_image_layout(command_buffer& command_buffer, const VkImage& image, VkFormat format, VkImageLayout src_image_layout, VkImageLayout dst_image_layout, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void;

  static auto insert_image_memory_barrier(command_buffer& command_buffer, const VkImage& image, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkImageAspectFlags image_aspect, uint32_t mip_levels, uint32_t base_mip_level, uint32_t layer_count, uint32_t base_array_layer) -> void;

  static auto copy_buffer_to_image(const VkBuffer& buffer, const VkImage& image, const VkExtent3D& extent, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void;

  static auto copy_image_to_buffer(const VkImage& image, VkFormat format, const VkBuffer& buffer, const VkOffset3D& offset, const VkExtent3D& extent, std::uint32_t mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void;

	static auto copy_image(const VkImage& src_image, VkImage& dst_image, VmaAllocation& dst_allocation, VkFormat src_format, const VkExtent3D& extent, VkImageLayout src_image_layout, std::uint32_t mip_level, std::uint32_t array_layer) -> bool;

  static auto channels_from_format(VkFormat format) -> std::uint8_t;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  auto extent() const noexcept -> const VkExtent3D&;

  auto size() const noexcept -> math::vector2u;

  auto format() const noexcept -> VkFormat;

  auto samples() const noexcept -> VkSampleCountFlagBits;

  auto usage() const noexcept -> VkImageUsageFlags;

  auto mip_levels() const noexcept -> std::uint32_t;

  auto array_layers() const noexcept -> std::uint32_t;

  auto filter() const noexcept -> VkFilter;

  auto address_mode() const noexcept -> VkSamplerAddressMode;

  auto layout() const noexcept -> VkImageLayout;

  auto handle() const noexcept -> VkImage;

  operator VkImage() const noexcept;

  auto view() const noexcept -> VkImageView;

  // auto memory() const noexcept -> const VkDeviceMemory&;

  auto sampler() const noexcept -> VkSampler;

  virtual auto name() const noexcept -> std::string {
    return "Image";
  }

protected:

  VkExtent3D _extent;

  VkFilter _filter;
  VkSamplerAddressMode _address_mode;

  VkSampleCountFlagBits _samples;
  VkImageLayout _layout;
  VkImageUsageFlags _usage;
  VkFormat _format;
  std::uint32_t _mip_levels;
  std::uint32_t _array_layers;

  VkImage _handle;
  VmaAllocation _allocation;
  // VkDeviceMemory _memory;
  VkImageView _view;
  VkSampler _sampler;

}; // class image

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_IMAGE_HPP_
