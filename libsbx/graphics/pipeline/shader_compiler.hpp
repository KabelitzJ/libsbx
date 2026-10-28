// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
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

  /** @brief A user attribute on a struct field, e.g. `[Range(0, 1)]`. Named without the `Attribute` suffix; integer arguments read as floats. */
  struct reflected_attribute {
    std::string name;
    std::vector<std::variant<std::float_t, std::string>> arguments;
  }; // struct reflected_attribute

  /** @brief One field of a reflected struct, in declaration order. */
  struct reflected_field {
    enum class kind : std::uint8_t {
      float_vector, // float, float2, float3 or float4
      texture2d,
      texture2d_array,
      structure,
      other
    }; // enum class kind

    std::string name;
    kind type{kind::other};
    std::uint32_t component_count{0u}; // float_vector only
    std::string type_name;             // Slang's name for the type, e.g. "float4"
    std::vector<reflected_attribute> attributes;
  }; // struct reflected_field

  struct module_reflection {
    std::vector<std::string> entry_points;
    std::unordered_map<std::string, std::vector<reflected_field>> structs;
    std::vector<std::filesystem::path> dependencies; // every file the module parsed: itself and its transitive #includes
  }; // struct module_reflection

  shader_compiler();

  ~shader_compiler();

  /**
   * @brief Compiles @p entry_points of @p path to SPIR-V, through the disk cache.
   *
   * @param path The shader to compile.
   * @param entry_points The entry points to compile and their stages.
   * @param material_loader Generated Slang appended to the source, with SBX_GENERATED_MATERIAL_LOADER defined ahead of it; empty for none. See shaders/material_properties.slang.
   *
   * @return One SPIR-V blob per entry point, in the order given.
   *
   * @throws utility::runtime_error If the file doesn't parse, an entry point is missing, or Slang fails to link or generate code.
   */
  [[nodiscard]] auto compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points, std::string_view material_loader = {}) -> std::vector<compiled_entry_point>;

  /**
   * @brief Reflects the push-constant struct @p entry_point sees in @p path. Throws on a field
   * type push_constant_field::kind can't represent. Parses the module again rather than riding on
   * compile(), whose disk-cache hits skip Slang's layout entirely.
   */
  [[nodiscard]] auto reflect_push_constants(const std::filesystem::path& path, const std::string& entry_point) -> push_constant_layout;

  /**
   * @brief Parses @p path and reports which of @p entry_points and @p structs it defines.
   *
   * @param path The shader to parse.
   * @param entry_points The entry point names to look for.
   * @param structs The struct names to reflect.
   *
   * @return The entry points that exist, in the order given, the fields of each struct that exists, and the files it depends on.
   *
   * @throws utility::runtime_error If the file can't be read or doesn't parse; the message carries Slang's diagnostics.
   */
  [[nodiscard]] auto reflect_module(const std::filesystem::path& path, std::span<const std::string_view> entry_points, std::span<const std::string_view> structs) -> module_reflection;

private:

  [[nodiscard]] auto _create_session(const std::filesystem::path& path, std::span<const slang::CompilerOptionEntry> options) -> Slang::ComPtr<slang::ISession>;

  [[nodiscard]] auto _load_module(slang::ISession& session, const std::filesystem::path& path, std::string_view material_loader = {}) -> slang::IModule*;

  // dependencies: one entry per file the module parsed (entry file plus transitive #includes via
  // slang::IModule::getDependencyFile*), each packed by the caller as "<path>\0<content>" — so
  // editing an included header changes the key too, instead of leaving a stale cache entry.
  [[nodiscard]] auto _cache_key(std::span<const std::string> dependencies, std::span<const entry_point_request> entry_points, std::span<const slang::CompilerOptionEntry> options, const char* profile) const -> std::string;

  // The global session isn't thread-safe; compile runs on the render thread, reflection on the main thread.
  std::mutex _mutex{};
  Slang::ComPtr<slang::IGlobalSession> _global_session{};
  shader_disk_cache _disk_cache{};

}; // class shader_compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
