// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/image.hpp>

#include <cmath>
#include <ranges>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/exit.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::graphics {

static const auto depth_formats = std::vector<VkFormat>{
  VK_FORMAT_D16_UNORM, 
  VK_FORMAT_X8_D24_UNORM_PACK32, 
  VK_FORMAT_D32_SFLOAT, 
  VK_FORMAT_D16_UNORM_S8_UINT,
  VK_FORMAT_D24_UNORM_S8_UINT,
  VK_FORMAT_D32_SFLOAT_S8_UINT
};

static const auto stencil_formats = std::vector<VkFormat>{
  VK_FORMAT_S8_UINT, 
  VK_FORMAT_D16_UNORM_S8_UINT, 
  VK_FORMAT_D24_UNORM_S8_UINT, 
  VK_FORMAT_D32_SFLOAT_S8_UINT
};

static constexpr auto anisotropy = 16.0f;

image::image(const VkExtent3D extent, VkFilter filter, VkSamplerAddressMode address_mode, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkFormat format, std::uint32_t mip_levels, std::uint32_t array_layers)
: _extent{extent},
  _filter{filter},
  _address_mode{address_mode},
  _samples{samples},
  // _layout{layout},
  _usage{usage},
  _format{format},
  _mip_levels{mip_levels},
  _array_layers{array_layers} { }

image::~image() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  auto& allocator = graphics_module.allocator();

  vkDestroyImageView(logical_device, _view, nullptr);
  vkDestroySampler(logical_device, _sampler, nullptr);
  vmaDestroyImage(allocator, _handle, _allocation);
  // vkFreeMemory(logical_device, _memory, nullptr);
  // vkDestroyImage(logical_device, _handle, nullptr);
}

auto image::create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags, std::uint32_t count) noexcept -> VkDescriptorSetLayoutBinding {
  auto descriptor_set_layout_binding = VkDescriptorSetLayoutBinding{};
  descriptor_set_layout_binding.binding = binding;
  descriptor_set_layout_binding.descriptorType = descriptor_type;
  descriptor_set_layout_binding.stageFlags = shader_stage_flags;
  descriptor_set_layout_binding.descriptorCount = count;
  descriptor_set_layout_binding.pImmutableSamplers = nullptr;

  return descriptor_set_layout_binding;
}

auto image::mip_levels(const VkExtent3D& extent) noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(std::floor(std::log2(std::max(extent.width, std::max(extent.height, extent.depth)))) + 1);
}

auto image::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) noexcept -> VkFormat {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
      
  auto& physical_device = graphics_module.physical_device();

  for (const auto& format : candidates) {
    auto format_properties = VkFormatProperties{};
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &format_properties);

    if (tiling == VK_IMAGE_TILING_LINEAR && (format_properties.linearTilingFeatures & features) == features) {
      return format;
    } 
    
    if (tiling == VK_IMAGE_TILING_OPTIMAL && (format_properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  return VK_FORMAT_UNDEFINED;
}

auto image::has_depth_component(VkFormat format) noexcept -> bool {
  return std::find(depth_formats.begin(), depth_formats.end(), format) != depth_formats.end();
}

auto image::has_stencil_component(VkFormat format) noexcept -> bool {
  return std::find(stencil_formats.begin(), stencil_formats.end(), format) != stencil_formats.end();
}

auto image::create_image(VkImage& image, VmaAllocation& allocation, const VkExtent3D& extent, VkFormat format, VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::uint32_t mip_levels, std::uint32_t array_layers, VkImageType type, VkImageCreateFlags flags) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& physical_device = graphics_module.physical_device();
  auto& logical_device = graphics_module.logical_device();

  auto& allocator = graphics_module.allocator();

  auto image_create_info = VkImageCreateInfo{};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.flags = flags;
  image_create_info.imageType = type;
  image_create_info.format = format;
  image_create_info.extent = extent;
  image_create_info.mipLevels = mip_levels;
  image_create_info.arrayLayers = array_layers;
  image_create_info.samples = samples;
  image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage = usage;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  auto allocation_create_info = VmaAllocationCreateInfo{};
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
  allocation_create_info.flags = 0u;
  // allocation_create_info.priority = 1.0f;

  static constexpr auto image_dedicated_threshold = VkDeviceSize{1024 * 1024};
  
  // Estimate image size: width * height * depth * array_layers * bytes_per_pixel * mip_levels_factor
  // Using a conservative estimate for bytes per pixel (4 bytes for most formats)
  const auto estimated_size = static_cast<VkDeviceSize>(extent.width) * static_cast<VkDeviceSize>(extent.height) * static_cast<VkDeviceSize>(extent.depth) * static_cast<VkDeviceSize>(array_layers) * 4u;
  
  if (estimated_size >= image_dedicated_threshold) {
    allocation_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }

  validate(vmaCreateImage(allocator, &image_create_info, &allocation_create_info, &image, &allocation, nullptr));

  // Optional: Set allocation name for debugging
  // vmaSetAllocationName(allocator, allocation, name().c_str());
}

auto image::create_image_view(const VkImage& image, VkImageView& image_view, VkImageViewType type, VkFormat format, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  auto image_view_create_info = VkImageViewCreateInfo{};
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.image = image;
  image_view_create_info.viewType = type;
  image_view_create_info.format = format;
  image_view_create_info.components = VkComponentMapping{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  image_view_create_info.subresourceRange.aspectMask = image_aspect;
  image_view_create_info.subresourceRange.baseMipLevel = base_mip_level;
  image_view_create_info.subresourceRange.levelCount = mip_levels;
  image_view_create_info.subresourceRange.baseArrayLayer = base_array_layer;
  image_view_create_info.subresourceRange.layerCount = layer_count;

  validate(vkCreateImageView(logical_device, &image_view_create_info, nullptr, &image_view));
}

auto image::create_image_sampler(VkSampler& sampler, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, std::uint32_t mip_levels) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& physical_device = graphics_module.physical_device();
  auto& logical_device = graphics_module.logical_device();

  auto sampler_create_info = VkSamplerCreateInfo{};
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.magFilter = filter;
  sampler_create_info.minFilter = filter;
  sampler_create_info.addressModeU = address_mode;
  sampler_create_info.addressModeV = address_mode;
  sampler_create_info.addressModeW = address_mode;
  sampler_create_info.mipmapMode = (filter == VK_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_create_info.mipLodBias = 0.0f;
  sampler_create_info.anisotropyEnable = anisotropic;
  sampler_create_info.maxAnisotropy = (anisotropic && logical_device.enabled_features().core.features.samplerAnisotropy) ? std::min(anisotropy, physical_device.properties().limits.maxSamplerAnisotropy) : 1.0f;
  sampler_create_info.compareEnable = VK_FALSE;
  sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_create_info.minLod = 0.0f;
  sampler_create_info.maxLod = static_cast<std::float_t>(mip_levels);
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_create_info.unnormalizedCoordinates = false;

  validate(vkCreateSampler(logical_device, &sampler_create_info, nullptr, &sampler));
}

auto image::create_mipmaps(const VkImage& image, const VkExtent3D& extent, VkFormat format, VkImageLayout dst_image_layout, std::uint32_t mip_levels, std::uint32_t base_array_layer, std::uint32_t layer_count) -> void {
  auto command_buffer = graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

  create_mipmaps(command_buffer, image, extent, format, dst_image_layout, mip_levels, base_array_layer, layer_count);

  command_buffer.submit_idle();
}

auto image::create_mipmaps(command_buffer& command_buffer, const VkImage& image, const VkExtent3D& extent, VkFormat format, VkImageLayout dst_image_layout, std::uint32_t mip_levels, std::uint32_t base_array_layer, std::uint32_t layer_count) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& physical_device = graphics_module.physical_device();

  auto format_properties = VkFormatProperties{};
  vkGetPhysicalDeviceFormatProperties(physical_device, format, &format_properties);

  if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
    throw std::runtime_error{"Texture image format does not support linear blitting"};
  }

  for (auto i : std::views::iota(1u, mip_levels)) {
    auto barrier0 = VkImageMemoryBarrier{};
    barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier0.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier0.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier0.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier0.image = image;
		barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier0.subresourceRange.baseMipLevel = i - 1;
		barrier0.subresourceRange.levelCount = 1;
		barrier0.subresourceRange.baseArrayLayer = base_array_layer;
		barrier0.subresourceRange.layerCount = layer_count;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier0);

    auto blit = VkImageBlit{};
    blit.srcOffsets[1] = {int32_t(extent.width >> (i - 1)), int32_t(extent.height >> (i - 1)), 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = base_array_layer;
		blit.srcSubresource.layerCount = layer_count;
		blit.dstOffsets[1] = {int32_t(extent.width >> i), int32_t(extent.height >> i), 1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = base_array_layer;
		blit.dstSubresource.layerCount = layer_count;

    vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    auto barrier1 = VkImageMemoryBarrier{};
    barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier1.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier1.newLayout = dst_image_layout;
		barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier1.image = image;
		barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier1.subresourceRange.baseMipLevel = i - 1;
		barrier1.subresourceRange.levelCount = 1;
		barrier1.subresourceRange.baseArrayLayer = base_array_layer;
		barrier1.subresourceRange.layerCount = layer_count;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);
  }

  auto barrier = VkImageMemoryBarrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = dst_image_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = base_array_layer;
	barrier.subresourceRange.layerCount = layer_count;

  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

auto image::transition_image_layout(const VkImage& image, VkFormat format, VkImageLayout src_image_layout, VkImageLayout dst_image_layout, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
  auto command_buffer = graphics::command_buffer{};

  transition_image_layout(command_buffer, image, format, src_image_layout, dst_image_layout, image_aspect, mip_levels, base_mip_level, layer_count, base_array_layer);

  command_buffer.submit_idle();
}

auto image::transition_image_layout(command_buffer& command_buffer, const VkImage& image, VkFormat /*format*/, VkImageLayout src_image_layout, VkImageLayout dst_image_layout, VkImageAspectFlags image_aspect, std::uint32_t mip_levels, std::uint32_t base_mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
  auto barrier = VkImageMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.oldLayout = src_image_layout;
  barrier.newLayout = dst_image_layout;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = image_aspect;
  barrier.subresourceRange.baseMipLevel = base_mip_level;
  barrier.subresourceRange.levelCount = mip_levels;
  barrier.subresourceRange.baseArrayLayer = base_array_layer;
  barrier.subresourceRange.layerCount = layer_count;

  // ---- source ----
  switch (src_image_layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = 0;
      break;
    }
    case VK_IMAGE_LAYOUT_GENERAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = 0;
      break;
    }
    default: {
      throw std::runtime_error{"Unsupported source image layout"};
    }
  }

  // ---- destination ----
  switch (dst_image_layout) {
    case VK_IMAGE_LAYOUT_GENERAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    }
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.dstAccessMask = 0;
      break;
    }
    default: {
      throw std::runtime_error{"Unsupported destination image layout"};
    }
  }

  auto dependency = VkDependencyInfo{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency);
}

auto image::insert_image_memory_barrier(command_buffer& command_buffer, const VkImage& image, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkImageAspectFlags image_aspect, uint32_t mip_levels, uint32_t base_mip_level, uint32_t layer_count, uint32_t base_array_layer) -> void {
  auto barrier = VkImageMemoryBarrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = src_access_mask;
	barrier.dstAccessMask = dst_access_mask;
	barrier.oldLayout = old_image_layout;
	barrier.newLayout = new_image_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect;
	barrier.subresourceRange.baseMipLevel = base_mip_level;
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = base_array_layer;
	barrier.subresourceRange.layerCount = layer_count;

  vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

auto image::copy_buffer_to_image(const VkBuffer& buffer, const VkImage& image, const VkExtent3D& extent, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
  auto command_buffer = graphics::command_buffer{};

  copy_buffer_to_image(command_buffer, buffer, image, extent, layer_count, base_array_layer);

	command_buffer.submit_idle();
}

auto image::copy_buffer_to_image(graphics::command_buffer& command_buffer, const VkBuffer& buffer, const VkImage& image, const VkExtent3D& extent, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
	auto region = VkBufferImageCopy{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = base_array_layer;
	region.imageSubresource.layerCount = layer_count;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = extent;

	vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

auto image::copy_image_to_buffer(const VkImage& image, VkFormat format, const VkBuffer& buffer, const VkOffset3D& offset, const VkExtent3D& extent, std::uint32_t mip_level, std::uint32_t layer_count, std::uint32_t base_array_layer) -> void {
  auto command_buffer = graphics::command_buffer{};

  transition_image_layout(command_buffer, image, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, layer_count, base_array_layer);

  auto region = VkBufferImageCopy{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = mip_level;
  region.imageSubresource.baseArrayLayer = base_array_layer;
  region.imageSubresource.layerCount = layer_count;
  region.imageOffset = offset;
  region.imageExtent = extent;

  vkCmdCopyImageToBuffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

  command_buffer.submit_idle();
}

auto image::copy_image(const VkImage& src_image, VkImage& dst_image, VmaAllocation& dst_allocation, VkFormat src_format, const VkExtent3D& extent, VkImageLayout src_image_layout, std::uint32_t mip_level, std::uint32_t array_layer) -> bool {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
  auto& physical_device = graphics_module.physical_device();
  auto& surface = graphics_module.surface();

	auto supports_blit = true;
	auto format_properties = VkFormatProperties{}; 

	vkGetPhysicalDeviceFormatProperties(physical_device, surface.format().format, &format_properties);

	if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
		utility::logger<"graphics">::warn("Device does not support blitting from optimal tiled images, using copy instead of blit");
		supports_blit = false;
	}

	vkGetPhysicalDeviceFormatProperties(physical_device, src_format, &format_properties);

	if (!(format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
		utility::logger<"graphics">::warn("Device does not support blitting to linear tiled images, using copy instead of blit");
		supports_blit = false;
	}

	create_image(dst_image, dst_allocation, extent, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, 1, VK_IMAGE_TYPE_2D);

	auto command_buffer = graphics::command_buffer{};

	insert_image_memory_barrier(command_buffer, dst_image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
	insert_image_memory_barrier(command_buffer, src_image, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, src_image_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1, mip_level, 1, array_layer);

	if (supports_blit) {
		auto blit_size = VkOffset3D{static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), static_cast<int32_t>(extent.depth)};

		auto image_blit_region = VkImageBlit{};
		image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit_region.srcSubresource.mipLevel = mip_level;
		image_blit_region.srcSubresource.baseArrayLayer = array_layer;
		image_blit_region.srcSubresource.layerCount = 1;
		image_blit_region.srcOffsets[1] = blit_size;
		image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit_region.dstSubresource.mipLevel = 0;
		image_blit_region.dstSubresource.baseArrayLayer = 0;
		image_blit_region.dstSubresource.layerCount = 1;
		image_blit_region.dstOffsets[1] = blit_size;

		vkCmdBlitImage(command_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit_region, VK_FILTER_NEAREST);
	} else {
		auto image_copy_region = VkImageCopy{};
		image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_copy_region.srcSubresource.mipLevel = mip_level;
		image_copy_region.srcSubresource.baseArrayLayer = array_layer;
		image_copy_region.srcSubresource.layerCount = 1;
		image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_copy_region.dstSubresource.mipLevel = 0;
		image_copy_region.dstSubresource.baseArrayLayer = 0;
		image_copy_region.dstSubresource.layerCount = 1;
		image_copy_region.extent = extent;

		vkCmdCopyImage(command_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy_region);
	}

	insert_image_memory_barrier(command_buffer, dst_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
	insert_image_memory_barrier(command_buffer, src_image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_image_layout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1, mip_level, 1, array_layer);

	command_buffer.submit_idle();

	return supports_blit;
}

auto image::channels_from_format(VkFormat format) -> std::uint8_t {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R64_UINT: {
      return 1;
    }
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_UINT: {
      return 2;
    }
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT: {
      return 3;
    }
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_R32G32B32A32_SFLOAT: {
      return 4;
    }
    default: {
      throw std::runtime_error{fmt::format("Unsupported image format: {}", static_cast<std::int32_t>(format))};
    }
  }
}

auto image::write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set {
  auto descriptor_image_infos = std::vector<VkDescriptorImageInfo>{};

  auto descriptor_image_info = VkDescriptorImageInfo{};
  descriptor_image_info.imageView = _view;
  descriptor_image_info.sampler = _sampler;

  switch (descriptor_type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
      descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      break;
    }
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    default: {
      descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      break;
    }
  }

  descriptor_image_infos.push_back(descriptor_image_info);

  auto descriptor_write = VkWriteDescriptorSet{};
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet = nullptr;
  descriptor_write.dstBinding = binding;
  descriptor_write.dstArrayElement = 0;
  descriptor_write.descriptorCount = 1;
  descriptor_write.descriptorType = descriptor_type;

  return graphics::write_descriptor_set{descriptor_write, descriptor_image_infos};
}

auto image::extent() const noexcept -> const VkExtent3D& {
  return _extent;
}

auto image::size() const noexcept -> math::vector2u {
  return math::vector2u{_extent.width, _extent.height};
}

auto image::format() const noexcept -> VkFormat {
  return _format;
}

auto image::samples() const noexcept -> VkSampleCountFlagBits {
  return _samples;
}

auto image::usage() const noexcept -> VkImageUsageFlags {
  return _usage;
}

auto image::mip_levels() const noexcept -> std::uint32_t {
  return _mip_levels;
}

auto image::array_layers() const noexcept -> std::uint32_t {
  return _array_layers;
}

auto image::filter() const noexcept -> VkFilter {
  return _filter;
}

auto image::address_mode() const noexcept -> VkSamplerAddressMode {
  return _address_mode;
}

// auto image::layout() const noexcept -> VkImageLayout {
//   return _layout;
// }

auto image::handle() const noexcept -> VkImage {
  return _handle;
}

image::operator VkImage() const noexcept {
  return _handle;
}

auto image::view() const noexcept -> VkImageView {
  return _view;
}

// auto image::memory() const noexcept -> VkDeviceMemory {
//   return _memory;
// }

auto image::sampler() const noexcept -> VkSampler {
  return _sampler;
}

} // namespace sbx::graphics
