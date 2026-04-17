// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/sampler_state.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/images/image.hpp>

namespace sbx::graphics {

sampler_state::sampler_state(graphics::filter mag_filter, graphics::filter min_filter, graphics::address_mode address_mode_u, graphics::address_mode address_mode_v, std::float_t anisotropy, std::float_t max_lod) {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& physical_device = graphics_module.physical_device();
  auto& logical_device = graphics_module.logical_device();

  const auto anisotropy_enabled = logical_device.enabled_features().core.features.samplerAnisotropy;
  const auto max_anisotropy = physical_device.properties().limits.maxSamplerAnisotropy;

  auto sampler_create_info = VkSamplerCreateInfo{};
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.magFilter = to_vk_enum<VkFilter>(mag_filter);
  sampler_create_info.minFilter = to_vk_enum<VkFilter>(min_filter);
  sampler_create_info.addressModeU = to_vk_enum<VkSamplerAddressMode>(address_mode_u);
  sampler_create_info.addressModeV = to_vk_enum<VkSamplerAddressMode>(address_mode_v);
  sampler_create_info.addressModeW = to_vk_enum<VkSamplerAddressMode>(address_mode_v);
  sampler_create_info.mipmapMode = (to_vk_enum<VkFilter>(min_filter) == VK_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_create_info.mipLodBias = 0.0f;
  sampler_create_info.anisotropyEnable = (anisotropy > 1.0f);
  sampler_create_info.maxAnisotropy = anisotropy_enabled ? std::min(anisotropy, max_anisotropy) : 1.0f;
  sampler_create_info.compareEnable = false;
  sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_create_info.minLod = 0.0f;
  sampler_create_info.maxLod = max_lod;
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_create_info.unnormalizedCoordinates = false;

  validate(vkCreateSampler(logical_device, &sampler_create_info, nullptr, &_handle));
}

sampler_state::sampler_state(graphics::filter filter, graphics::address_mode address_mode)
: sampler_state{filter, filter, address_mode, address_mode} { }

sampler_state::~sampler_state() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  vkDestroySampler(logical_device, _handle, nullptr);
}

auto sampler_state::create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding {
  auto descriptor_set_layout_binding = VkDescriptorSetLayoutBinding{};
  descriptor_set_layout_binding.binding = binding;
  descriptor_set_layout_binding.descriptorType = descriptor_type;
  descriptor_set_layout_binding.stageFlags = shader_stage_flags;
  descriptor_set_layout_binding.descriptorCount = 1u;
  descriptor_set_layout_binding.pImmutableSamplers = nullptr;

  return descriptor_set_layout_binding;
}

auto sampler_state::write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set {
  auto descriptor_image_infos = std::vector<VkDescriptorImageInfo>{};

  auto descriptor_image_info = VkDescriptorImageInfo{};
  descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  descriptor_image_info.imageView = nullptr;
  descriptor_image_info.sampler = _handle;

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

auto sampler_state::handle() const noexcept -> VkSampler {
  return _handle;
}

sampler_state::operator VkSampler() const noexcept {
  return _handle;
}

} // namespace sbx::graphics
