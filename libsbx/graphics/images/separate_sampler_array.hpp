// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_SAMPLERS_SAMPLER_STATE_ARRAY_HPP_
#define LIBSBX_GRAPHICS_SAMPLERS_SAMPLER_STATE_ARRAY_HPP_

#include <vector>
#include <unordered_map>

#include <libsbx/graphics/descriptor/descriptor.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

namespace sbx::graphics {

class separate_sampler_array : public descriptor {

public:

  inline static constexpr auto max_size = std::uint32_t{16u};

  using handle_type = sampler_state_handle;

  separate_sampler_array();

  ~separate_sampler_array();

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  auto push_back(const handle_type& handle) -> std::uint32_t;

  auto clear() -> void;

private:

  std::vector<handle_type> _sampler_ids;
  std::unordered_map<handle_type, std::uint32_t> _id_to_indices;

}; // class separate_sampler_array

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_SAMPLERS_SAMPLER_STATE_ARRAY_HPP_
