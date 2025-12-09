#ifndef LIBSBX_GRAPHICS_IMAGES_SAMPLER_STATE_HPP_
#define LIBSBX_GRAPHICS_IMAGES_SAMPLER_STATE_HPP_

#include <libsbx/graphics/descriptor/descriptor.hpp>
#include <libsbx/graphics/resource_storage.hpp>
#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::graphics {

class sampler_state : public descriptor {

public:

  sampler_state(graphics::filter mag_filter, graphics::filter min_filter, graphics::address_mode address_mode_u, graphics::address_mode address_mode_v, std::float_t anisotropy = 1.0f);

  sampler_state(graphics::filter filter = graphics::filter::linear, graphics::address_mode address_mode = graphics::address_mode::clamp_to_edge);

  ~sampler_state();

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  auto handle() const noexcept -> VkSampler;

  operator VkSampler() const noexcept;

private:

  VkSampler _handle;

}; // class sampler_state

using sampler_state_handle = resource_handle<sampler_state>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_SAMPLER_STATE_HPP_
