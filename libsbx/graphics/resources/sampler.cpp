// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/resources/sampler.hpp>

#include <algorithm>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

sampler::sampler(const create_info& create_info) {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& physical_device = graphics_module.physical_device();

  const auto& properties = physical_device.properties();

  const auto max_device_anisotropy = properties.limits.maxSamplerAnisotropy;
  const auto anisotropy = std::min(create_info.max_anisotropy, max_device_anisotropy);
  const auto anisotropy_enabled = anisotropy > 1.0f;

  auto sampler_create_info = VkSamplerCreateInfo{};
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.magFilter = to_vk_enum<VkFilter>(create_info.mag_filter);
  sampler_create_info.minFilter = to_vk_enum<VkFilter>(create_info.min_filter);
  sampler_create_info.mipmapMode = to_vk_enum<VkSamplerMipmapMode>(create_info.mipmap_mode);
  sampler_create_info.addressModeU = to_vk_enum<VkSamplerAddressMode>(create_info.address_mode_u);
  sampler_create_info.addressModeV = to_vk_enum<VkSamplerAddressMode>(create_info.address_mode_v);
  sampler_create_info.addressModeW = to_vk_enum<VkSamplerAddressMode>(create_info.address_mode_w);
  sampler_create_info.anisotropyEnable = anisotropy_enabled;
  sampler_create_info.maxAnisotropy = anisotropy_enabled ? anisotropy : 1.0f;
  sampler_create_info.minLod = create_info.min_lod;
  sampler_create_info.maxLod = create_info.max_lod;
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  sampler_create_info.compareEnable = false;
  sampler_create_info.unnormalizedCoordinates = false;

  validate(vkCreateSampler(logical_device, &sampler_create_info, nullptr, &_handle), "vkCreateSampler");

  logical_device.set_debug_name(_handle, create_info.name);
}

sampler::sampler(sampler&& other) noexcept
: _handle{std::exchange(other._handle, nullptr)} { }

sampler::~sampler() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  vkDestroySampler(graphics_module.logical_device(), _handle, nullptr);
}

auto sampler::operator=(sampler&& other) noexcept -> sampler& {
  if (this != &other) {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& logical_device = graphics_module.logical_device();

    vkDestroySampler(logical_device, _handle, nullptr);

    _handle = std::exchange(other._handle, nullptr);
  }

  return *this;
}

[[nodiscard]] auto sampler::handle() const noexcept -> handle_type {
  return _handle;
}

sampler::operator handle_type() const noexcept {
  return _handle;
}

} // namespace sbx::graphics
