#ifndef LIBSBX_GRAPHICS_IMAGES_SEPARATE_SAMPLER_HPP_
#define LIBSBX_GRAPHICS_IMAGES_SEPARATE_SAMPLER_HPP_

#include <libsbx/graphics/descriptor/descriptor.hpp>

namespace sbx::graphics {

class separate_sampler : public descriptor {

public:

  separate_sampler(VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  ~separate_sampler();

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  auto handle() const noexcept -> VkSampler;

  operator VkSampler() const noexcept;

private:

  VkSampler _handle;

}; // class separate_sampler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_SEPARATE_SAMPLER_HPP_
