// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_IMAGES_CUBE_IMAGE_HPP_
#define LIBSBX_GRAPHICS_IMAGES_CUBE_IMAGE_HPP_

#include <filesystem>

#include <libsbx/utility/timer.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/images/image.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>

#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

class cube_image : public image {

public:

  cube_image(const std::filesystem::path& path, const std::string& suffix = ".png", VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool anisotropic = true, bool mipmap = true);

  cube_image(const math::vector2u& extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

  ~cube_image() override;

  auto name() const noexcept -> std::string override {
    return "Cube Image";
  }

private:

  auto _load(const std::filesystem::path& path = {}, const std::string& suffix = {}) -> void;

  inline static constexpr auto side_names = std::array<std::string_view, 6u>{"right", "left", "top", "bottom", "front", "back"};

  bool _anisotropic;
  bool _mipmap;
  std::uint8_t _channels;

}; // class cube_image

using cube_image2d_handle = resource_handle<cube_image>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_CUBE_IMAGE_HPP_
