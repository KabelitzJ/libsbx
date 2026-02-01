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

  cube_image(const math::vector2u& extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

  ~cube_image() override;

  auto name() const noexcept -> std::string override {
    return "Cube Image";
  }

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override {
    auto descriptor_image_infos = std::vector<VkDescriptorImageInfo>{};

    auto descriptor_image_info = VkDescriptorImageInfo{};
    descriptor_image_info.imageLayout = (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor_image_info.imageView = (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ? _array_view : _view;
    descriptor_image_info.sampler = _sampler;

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

private:

  auto _load(const std::filesystem::path& path = {}, const std::string& suffix = {}) -> void;

  inline static constexpr auto side_names = std::array<std::string_view, 6u>{"right", "left", "top", "bottom", "front", "back"};

  bool _anisotropic;
  bool _mipmap;
  std::uint8_t _channels;

  VkImageView _array_view;

}; // class cube_image

using cube_image2d_handle = resource_handle<cube_image>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_CUBE_IMAGE_HPP_
