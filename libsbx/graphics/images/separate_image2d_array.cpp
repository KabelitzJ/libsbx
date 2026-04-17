// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/separate_image2d_array.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

separate_image2d_array::separate_image2d_array() {

}

separate_image2d_array::~separate_image2d_array() {

}

auto separate_image2d_array::create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding {
  auto descriptor_set_layout_binding = VkDescriptorSetLayoutBinding{};
  descriptor_set_layout_binding.binding = binding;
  descriptor_set_layout_binding.descriptorType = descriptor_type;
  descriptor_set_layout_binding.stageFlags = shader_stage_flags;
  descriptor_set_layout_binding.descriptorCount = max_size;
  descriptor_set_layout_binding.pImmutableSamplers = nullptr;

  return descriptor_set_layout_binding;
}

auto separate_image2d_array::write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set {
  auto descriptor_image_infos = std::vector<VkDescriptorImageInfo>{};

  for (const auto view : _image_views) {
    auto descriptor_image_info = VkDescriptorImageInfo{};
    descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor_image_info.imageView = view;
    descriptor_image_info.sampler = nullptr;
    
    descriptor_image_infos.push_back(descriptor_image_info);
  }

  auto descriptor_write = VkWriteDescriptorSet{};
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet = nullptr;
  descriptor_write.dstBinding = binding;
  descriptor_write.dstArrayElement = 0;
  descriptor_write.descriptorCount = static_cast<std::uint32_t>(descriptor_image_infos.size());
  descriptor_write.descriptorType = descriptor_type;

  return graphics::write_descriptor_set{descriptor_write, descriptor_image_infos};
}

auto separate_image2d_array::push_back(const handle_type& handle) -> std::uint32_t {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_image_views.size() > max_size) {
    throw std::runtime_error{"separate_image2d_array::push_back: max_size exceeded"};
  }

  if (!handle.is_valid()) {
    return max_size;
  }

  auto& image = graphics_module.get_resource<graphics::image2d>(handle);
  auto view = image.view();

  if (const auto entry = _view_to_indices.find(view); entry != _view_to_indices.cend()) {
    return entry->second;
  }

  const auto index = static_cast<std::uint32_t>(_image_views.size());

  _image_views.push_back(view);
  _view_to_indices.emplace(view, index);

  return index;
}

auto separate_image2d_array::push_back(const std::string& attachment) -> std::uint32_t {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_image_views.size() > max_size) {
    throw std::runtime_error{"separate_image2d_array::push_back: max_size exceeded"};
  }

  const auto& image = static_cast<const image2d&>(graphics_module.attachment(attachment));
  auto view = image.view();

  if (const auto entry = _view_to_indices.find(view); entry != _view_to_indices.cend()) {
    return entry->second;
  }

  const auto index = static_cast<std::uint32_t>(_image_views.size());

  _image_views.push_back(view);
  _view_to_indices.emplace(view, index);

  return index;
}

auto separate_image2d_array::clear() -> void {
  _image_views.clear();
  _view_to_indices.clear();
}

} // namespace sbx::graphics
