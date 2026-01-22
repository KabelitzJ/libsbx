// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

static const auto depth_formats = std::vector<VkFormat>{
  VK_FORMAT_D32_SFLOAT_S8_UINT,
  VK_FORMAT_D32_SFLOAT,
  VK_FORMAT_D24_UNORM_S8_UINT,
  VK_FORMAT_D16_UNORM_S8_UINT,
  VK_FORMAT_D16_UNORM
};

depth_image::depth_image(const math::vector2u& extent, VkSampleCountFlagBits samples)
: image{VkExtent3D{extent.x(), extent.y(), 1}, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, samples, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, format(), 1, 1} {
  if (_format == VK_FORMAT_UNDEFINED) {
    throw std::runtime_error{"Failed to find supported depth format"};
  }

  auto aspect_mask = VkImageAspectFlags{VK_IMAGE_ASPECT_DEPTH_BIT};

  if (has_stencil_component(_format)) {
    aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  } else {
    utility::logger<"graphics">::warn("Depth format '{}' does not have a stencil component", _format);
  }

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 1, VK_IMAGE_TYPE_2D);
	create_image_sampler(_sampler, _filter, _address_mode, false, 1);
	create_image_view(_handle, _view, VK_IMAGE_VIEW_TYPE_2D, _format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 0, 1, 0);
	transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, _layout, aspect_mask, 1, 0, 1, 0);
}

auto depth_image::format() -> VkFormat {
  return find_supported_format(depth_formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

} // namespace sbx::graphics
