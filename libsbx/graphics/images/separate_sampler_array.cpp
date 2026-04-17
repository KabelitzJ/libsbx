// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/separate_sampler_array.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

separate_sampler_array::separate_sampler_array() {

}

separate_sampler_array::~separate_sampler_array() {

}

auto separate_sampler_array::create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding {
  auto descriptor_set_layout_binding = VkDescriptorSetLayoutBinding{};
  descriptor_set_layout_binding.binding = binding;
  descriptor_set_layout_binding.descriptorType = descriptor_type;
  descriptor_set_layout_binding.stageFlags = shader_stage_flags;
  descriptor_set_layout_binding.descriptorCount = max_size;
  descriptor_set_layout_binding.pImmutableSamplers = nullptr;

  return descriptor_set_layout_binding;
}

auto separate_sampler_array::write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto descriptor_image_infos = std::vector<VkDescriptorImageInfo>{};

  for (const auto id : _sampler_ids) {
    auto& sampler_state = graphics_module.get_resource<graphics::sampler_state>(id);

    auto descriptor_image_info = VkDescriptorImageInfo{};
    descriptor_image_info.sampler = sampler_state;
    descriptor_image_info.imageView = nullptr;
    descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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

auto separate_sampler_array::push_back(const handle_type& handle) -> std::uint32_t {
  if (_sampler_ids.size() > max_size) {
    throw std::runtime_error{"separate_sampler_array::push_back: max_size exceeded"};
  }

  if (!handle.is_valid()) {
    return max_size;
  }

  if (const auto entry = _id_to_indices.find(handle); entry != _id_to_indices.cend()) {
    return entry->second;
  }

  const auto index = static_cast<std::uint32_t>(_sampler_ids.size());

  _sampler_ids.push_back(handle);
  _id_to_indices.emplace(handle, index);

  return index;
}

auto separate_sampler_array::clear() -> void {
  _sampler_ids.clear();
  _id_to_indices.clear();
}

} // namespace sbx::graphics
