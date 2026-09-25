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

  /**
   * @brief One field of a shader's `[[vk::push_constant]]` struct. Texture/sampler kinds are
   * recognized by the handle struct types declared in shaders/script_compute.slang.
   */
  struct push_constant_field {
    enum class kind : std::uint8_t {
      f32,
      i32,
      u32,
      f32x2,
      f32x3,
      f32x4,
      sampled_texture,
      storage_texture,
      sampler,
      buffer
    }; // enum class kind

    std::string name;
    std::uint32_t offset;
    std::uint32_t size;
    kind type;
    std::uint32_t element_stride{0u}; // buffer only: stride of the pointee type
  }; // struct push_constant_field

  struct push_constant_layout {
    std::uint32_t size{0u};
    std::vector<push_constant_field> fields{};
  }; // struct push_constant_layout

  shader_compiler();

  ~shader_compiler();

  [[nodiscard]] auto compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points) -> std::vector<compiled_entry_point>;

  /**
   * @brief Reflects the push-constant struct @p entry_point sees in @p path. Throws on a field
   * type push_constant_field::kind can't represent. Parses the module again rather than riding on
   * compile(), whose disk-cache hits skip Slang's layout entirely.
   */
  [[nodiscard]] auto reflect_push_constants(const std::filesystem::path& path, const std::string& entry_point) -> push_constant_layout;

private:

  [[nodiscard]] auto _create_session(const std::filesystem::path& path, std::span<const slang::CompilerOptionEntry> options) -> Slang::ComPtr<slang::ISession>;

  [[nodiscard]] auto _load_module(slang::ISession& session, const std::filesystem::path& path) -> slang::IModule*;

  // dependencies: one entry per file the module parsed (entry file plus transitive #includes via
  // slang::IModule::getDependencyFile*), each packed by the caller as "<path>\0<content>" — so
  // editing an included header changes the key too, instead of leaving a stale cache entry.
  [[nodiscard]] auto _cache_key(std::span<const std::string> dependencies, std::span<const entry_point_request> entry_points, std::span<const slang::CompilerOptionEntry> options, const char* profile) const -> std::string;

  Slang::ComPtr<slang::IGlobalSession> _global_session{};
  shader_disk_cache _disk_cache{};

}; // class shader_compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
