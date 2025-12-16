#include <libsbx/graphics/render_pass/swapchain.hpp>

#include <limits>
#include <ranges>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

static const auto composite_alpha_flags = std::vector<VkCompositeAlphaFlagBitsKHR>{
	VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, 
  VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
	VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, 
  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
};

swapchain::swapchain(const std::unique_ptr<swapchain>& old_swapchain)
: _present_mode{VK_PRESENT_MODE_FIFO_KHR},
  _active_image_index{std::numeric_limits<std::uint32_t>::max()},
  _pre_transform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR},
  _composite_alpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& physical_device = graphics_module.physical_device();
  const auto& surface = graphics_module.surface();

  const auto surface_capabilities = surface.capabilities();
  const auto& surface_format = surface.format();

  const auto& graphics_queue = logical_device.queue<queue::type::graphics>();
  const auto& present_queue = logical_device.queue<queue::type::present>();

  _present_mode = _choose_present_mode();
  
  // Choose number of images in the swapchain - aim for 3, but respect min/max
  auto desired_image_count = std::uint32_t{3};

  if (surface_capabilities.minImageCount > desired_image_count) {
    desired_image_count = surface_capabilities.minImageCount;
  }

	if (surface_capabilities.maxImageCount > 0 && desired_image_count > surface_capabilities.maxImageCount) {
		desired_image_count = surface_capabilities.maxImageCount;
	}

	if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		_pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		_pre_transform = surface_capabilities.currentTransform;
	}

	for (const auto& composite_alpha_flag : composite_alpha_flags) {
		if (surface_capabilities.supportedCompositeAlpha & composite_alpha_flag) {
			_composite_alpha = composite_alpha_flag;
			break;
		}
	}

  auto capabilities = VkSurfaceCapabilitiesKHR{};

  validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities));

  _extent = capabilities.currentExtent;
  _format = surface_format.format;

	auto swapchain_create_info = VkSwapchainCreateInfoKHR{};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = surface;
	swapchain_create_info.minImageCount = desired_image_count;
	swapchain_create_info.imageFormat = surface_format.format;
	swapchain_create_info.imageColorSpace = surface_format.colorSpace;
	swapchain_create_info.imageExtent = _extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_create_info.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(_pre_transform);
	swapchain_create_info.compositeAlpha = _composite_alpha;
	swapchain_create_info.presentMode = _present_mode;
	swapchain_create_info.clipped = true;
  swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
		swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    utility::logger<"graphics">::debug("Swapchain supports VK_IMAGE_USAGE_TRANSFER_SRC_BIT");
  }

	if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
		swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    utility::logger<"graphics">::debug("Swapchain supports VK_IMAGE_USAGE_TRANSFER_DST_BIT");
  } else {
    throw std::runtime_error("Swapchain does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT");
  }

	if (old_swapchain) {
    swapchain_create_info.oldSwapchain = *old_swapchain;
  }

  if (present_queue.family() != graphics_queue.family()) {
    auto queue_families = std::array<std::uint32_t, 2>{graphics_queue.family(), present_queue.family()};

    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = static_cast<std::uint32_t>(queue_families.size());
    swapchain_create_info.pQueueFamilyIndices = queue_families.data();
  }

	validate(vkCreateSwapchainKHR(logical_device, &swapchain_create_info, nullptr, &_handle));

  utility::logger<"graphics">::debug("Created swapchain ({}x{}) with {} images", _extent.width, _extent.height, desired_image_count);

	validate(vkGetSwapchainImagesKHR(logical_device, _handle, &_image_count, nullptr));

	_images.resize(_image_count);
  _image_views.resize(_image_count);

	validate(vkGetSwapchainImagesKHR(logical_device, _handle, &_image_count, _images.data()));

	for (uint32_t i = 0; i < _image_count; i++) {
    auto& image = _images.at(i);
    auto& image_view = _image_views.at(i);

    _create_image_view(image, surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT, image_view);
	}
}

swapchain::~swapchain() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

	const auto& logical_device = graphics_module.logical_device();

	for (const auto& image_view : _image_views) {
		vkDestroyImageView(logical_device, image_view, nullptr);
	}

	vkDestroySwapchainKHR(logical_device, _handle, nullptr);
}

auto swapchain::handle() const noexcept -> const VkSwapchainKHR& {
  return _handle;
}

swapchain::operator const VkSwapchainKHR&() const noexcept {
  return _handle;
}

auto swapchain::extent() const noexcept -> const VkExtent2D& {
  return _extent;
}

auto swapchain::active_image_index() const noexcept -> std::uint32_t {
  return _active_image_index;
}

auto swapchain::image_count() const noexcept -> std::uint32_t {
  return _image_count;
}

auto swapchain::pre_transform() const noexcept -> VkSurfaceTransformFlagsKHR {
  return _pre_transform;
}

auto swapchain::composite_alpha() const noexcept -> VkCompositeAlphaFlagBitsKHR {
  return _composite_alpha;
}

auto swapchain::present_mode() const noexcept -> VkPresentModeKHR {
  return _present_mode;
}

auto swapchain::image(std::uint32_t index) const noexcept -> const VkImage& {
  return _images.at(index);
}

auto swapchain::image_view(std::uint32_t index) const noexcept -> const VkImageView& {
  return _image_views.at(index);
}

auto swapchain::acquire_next_image(const VkSemaphore& image_available_semaphore, const VkFence& fence) -> VkResult {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
        
  auto& logical_device = graphics_module.logical_device();

  if (fence) {
    validate(vkWaitForFences(logical_device, 1, &fence, true, std::numeric_limits<std::uint64_t>::max()));
  }

  const auto result = vkAcquireNextImageKHR(logical_device, _handle, std::numeric_limits<std::uint64_t>::max(), image_available_semaphore, nullptr, &_active_image_index);

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
    throw std::runtime_error("Failed to acquire next image");
  }

  return result;
}

auto swapchain::present(const VkSemaphore& wait_semaphore) -> VkResult {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
        
  auto& logical_device = graphics_module.logical_device();

  const auto& present_queue = logical_device.queue<queue::type::present>();

  auto present_info = VkPresentInfoKHR{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &wait_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &_handle;
	present_info.pImageIndices = &_active_image_index;

	return vkQueuePresentKHR(present_queue, &present_info);
}

auto swapchain::_choose_present_mode() const -> VkPresentModeKHR {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& physical_device = graphics_module.physical_device();
  const auto& surface = graphics_module.surface();

  auto physical_present_mode_count = std::uint32_t{0};
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &physical_present_mode_count, nullptr);

  auto physical_present_modes = utility::make_vector<VkPresentModeKHR>(physical_present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &physical_present_mode_count, physical_present_modes.data());

  for (const auto& present_mode : physical_present_modes) {
		if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      utility::logger<"graphics">::debug("Using VK_PRESENT_MODE_MAILBOX_KHR");
			return present_mode;
		}
	}

  utility::logger<"graphics">::debug("Using VK_PRESENT_MODE_FIFO_KHR");

  return VK_PRESENT_MODE_FIFO_KHR;
}

auto swapchain::_create_image_view(const VkImage& image, VkFormat format, VkImageAspectFlags aspect, VkImageView& image_view) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto image_view_create_info = VkImageViewCreateInfo{};
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.image = image;
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = format;
  image_view_create_info.subresourceRange.aspectMask = aspect;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = 1;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = 1;

  validate(vkCreateImageView(logical_device, &image_view_create_info, nullptr, &image_view));
}

} // namespace sbx::graphics
