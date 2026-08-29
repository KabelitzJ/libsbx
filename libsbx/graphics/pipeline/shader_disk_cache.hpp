// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_

#include <cstdint>
#include <filesystem>
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

  /**
   * @brief @p source is the shader's own source file — used only to decide *where* to look
   * (the engine's shared cache, or the active project's own), never part of @p key itself. See
   * shader_disk_cache.cpp's cache_path().
   */
  [[nodiscard]] auto try_load(const std::string& key, const std::filesystem::path& source) const -> std::optional<std::vector<shader_binary_entry>>;

  auto store(const std::string& key, const std::filesystem::path& source, const std::vector<shader_binary_entry>& compiled) const -> void;

}; // class shader_disk_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_DISK_CACHE_HPP_
