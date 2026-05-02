// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_TEXTURE_CACHE_HPP_
#define EDITOR_PANELS_TEXTURE_CACHE_HPP_

#include <unordered_map>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/images/image2d.hpp>

namespace editor {

class texture_cache {

  struct image_handle_hash {

    auto operator()(const sbx::graphics::image2d_handle& handle) const noexcept -> std::size_t {
      return (static_cast<std::size_t>(handle.handle()) << 8) ^ static_cast<std::size_t>(handle.generation());
    }

  }; // struct image_handle_hash

  struct entry {
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
    VkImageView cached_view{VK_NULL_HANDLE};
  }; // struct entry

public:

  texture_cache() = default;

  ~texture_cache();

  texture_cache(const texture_cache&) = delete;

  auto operator=(const texture_cache&) -> texture_cache& = delete;

  auto descriptor_for(const sbx::graphics::image2d_handle& handle) -> VkDescriptorSet;

  auto release_all() -> void;

private:

  std::unordered_map<sbx::graphics::image2d_handle, entry, image_handle_hash> _entries;

}; // class texture_cache

} // namespace editor

#endif // EDITOR_PANELS_TEXTURE_CACHE_HPP_
