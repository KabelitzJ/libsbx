// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/pipeline/shader_disk_cache.hpp>

namespace sbx::graphics {

/**
 * @brief Compiles Slang source to SPIR-V at runtime via the Slang API.
 *
 * One global session for the process. A module may hold several entry points (one file, a vertex
 * and a fragment function); the caller names the ones it wants and their stages, and gets a
 * SPIR-V blob per entry point.
 */
class shader_compiler : public utility::noncopyable {

public:

  struct entry_point_request {
    VkShaderStageFlagBits stage;
    std::string name;
    std::optional<std::string> specialization{};

    auto operator==(const entry_point_request&) const -> bool = default;

  }; // struct entry_point_request

  struct compiled_entry_point {
    VkShaderStageFlagBits stage;
    std::string name;
    std::vector<std::uint32_t> spirv;
  }; // struct compiled_entry_point

  shader_compiler();

  ~shader_compiler();

  [[nodiscard]] auto compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points) -> std::vector<compiled_entry_point>;

private:

  // dependencies: one entry per file the loaded module actually parsed (the requested entry file
  // plus everything transitively reached via #include, per slang::IModule::getDependencyFile*),
  // each already packed as "<path>\0<content>" by the caller — not just the entry file's own text,
  // so editing a shared header the module includes changes the key too instead of leaving a stale
  // cache entry in place.
  [[nodiscard]] auto _cache_key(std::span<const std::string> dependencies, std::span<const entry_point_request> entry_points, std::span<const slang::CompilerOptionEntry> options, const char* profile) const -> std::string;

  Slang::ComPtr<slang::IGlobalSession> _global_session{};
  shader_disk_cache _disk_cache{};

}; // class shader_compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
