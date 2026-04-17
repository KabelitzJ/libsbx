// SPDX-License-Identifier: MIT
#include <libsbx/graphics/descriptor/descriptor.hpp>

namespace sbx::graphics {

write_descriptor_set::write_descriptor_set(const VkWriteDescriptorSet& write_descriptor_set, const std::vector<VkDescriptorImageInfo>& descriptor_infos) noexcept
: _write_descriptor_set{write_descriptor_set},
  _descriptor_image_infos{descriptor_infos} {
  _write_descriptor_set.pImageInfo = _descriptor_image_infos.data();
  _write_descriptor_set.pBufferInfo = nullptr;
}

write_descriptor_set::write_descriptor_set(const VkWriteDescriptorSet& write_descriptor_set, const VkDescriptorBufferInfo& descriptor_info) noexcept
: _write_descriptor_set{write_descriptor_set}, 
  _descriptor_buffer_info{std::make_unique<VkDescriptorBufferInfo>(descriptor_info)} { 
  _write_descriptor_set.pBufferInfo = _descriptor_buffer_info.get();
  _write_descriptor_set.pImageInfo = nullptr;
}

auto write_descriptor_set::handle() const noexcept -> const VkWriteDescriptorSet& {
  return _write_descriptor_set;
}

write_descriptor_set::operator const VkWriteDescriptorSet&() const noexcept {
  return _write_descriptor_set;
}

} // namespace sbx::graphics
