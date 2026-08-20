// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

struct shader_binary_entry {
  VkShaderStageFlagBits stage;
  std::string name;
  std::vector<std::uint32_t> spirv;
}; // struct shader_binary_entry

class shader_disk_cache : public utility::noncopyable {

public:

  shader_disk_cache() = default;

  ~shader_disk_cache() = default;

  [[nodiscard]] auto try_load(const std::string& key) const -> std::optional<std::vector<shader_binary_entry>>;

  auto store(const std::string& key, const std::vector<shader_binary_entry>& compiled) const -> void;

}; // class shader_disk_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_
