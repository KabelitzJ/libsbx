// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_IMAGES_SEPARATE_IMAGE2D_ARRAY_HPP_
#define LIBSBX_GRAPHICS_IMAGES_SEPARATE_IMAGE2D_ARRAY_HPP_

#include <vector>
#include <unordered_map>

#include <libsbx/graphics/resource_storage.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::graphics {

class separate_image2d_array : public descriptor {

public:

  inline static constexpr auto max_size = std::uint32_t{256u};

  using handle_type = image2d_handle;

  separate_image2d_array();

  ~separate_image2d_array();

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags shader_stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  auto push_back(const handle_type& handle) -> std::uint32_t;

  auto clear() -> void;

private:

  std::vector<handle_type> _image_ids;
  std::unordered_map<handle_type, std::uint32_t> _id_to_indices;

}; // class separate_image2d_array

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_SEPARATE_IMAGE2D_ARRAY_HPP_
