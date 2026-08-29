// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/shader_disk_cache.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/user_data_directory.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/fourcc.hpp>

namespace sbx::graphics {

inline constexpr auto shader_cache_magic = utility::fourcc_v<"SBSC">; // 'SBSC'
inline constexpr auto shader_cache_format_version = std::uint32_t{1u};

struct shader_cache_header {
  std::uint32_t magic;
  std::uint32_t format_version;
  std::uint32_t key_length;
  std::uint32_t entry_point_count;
}; // struct shader_cache_header

struct entry_record_header {
  std::uint32_t stage;
  std::uint32_t name_length;
  std::uint32_t spirv_word_count;
}; // struct entry_record_header

/**
 * @brief True if @p source (a shader's own source file) lives inside the active project's
 * assets_directory() — i.e. it's a project-authored shader, not an engine-shipped one. No
 * project-authored shaders exist yet (every call site today passes a path under the engine's own
 * fixed shaders/ tree), so this is always false in practice for now; the check exists so the
 * split is already correct once that changes. weakly_canonical + lexically_relative rather than a
 * string-prefix compare, so it can't be fooled by a sibling directory sharing a name prefix
 * (e.g. "assets2") and correctly rejects a ".."-escape out of the assets tree.
 */
auto _is_project_shader(const std::filesystem::path& source) -> bool {
  if (!core::engine::has_project()) {
    return false;
  }

  auto error = std::error_code{};

  const auto canonical_source = std::filesystem::weakly_canonical(source, error);

  if (error) {
    return false;
  }

  const auto canonical_assets = std::filesystem::weakly_canonical(core::engine::project().assets_directory(), error);

  if (error) {
    return false;
  }

  const auto relative = canonical_source.lexically_relative(canonical_assets);

  return !relative.empty() && *relative.begin() != "..";
}

auto cache_path(const std::string& key, const std::filesystem::path& source) -> std::filesystem::path {
  if (_is_project_shader(source)) {
    return core::engine::project().library_directory() / "shaders" / (key + ".spvcache");
  }

  // Engine-shipped shader (today, every shader) — shared globally across every app/project.
  // See shader_compiler.cpp's cache key: content-hashed, so a shared cache can only ever hit for
  // byte-identical source, never serve something stale.
  return core::user_data_directory() / "cache" / "shaders" / (key + ".spvcache");
}

auto shader_disk_cache::try_load(const std::string& key, const std::filesystem::path& source) const -> std::optional<std::vector<shader_binary_entry>> {
  auto in = std::ifstream{cache_path(key, source), std::ios::binary};

  if (!in) {
    return std::nullopt;
  }

  auto header = shader_cache_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != shader_cache_magic || header.format_version != shader_cache_format_version) {
    return std::nullopt;
  }

  auto stored_key = std::string(header.key_length, '\0');
  in.read(stored_key.data(), static_cast<std::streamsize>(header.key_length));

  if (!in || stored_key != key) {
    return std::nullopt;
  }

  auto results = std::vector<shader_binary_entry>{};
  results.reserve(header.entry_point_count);

  for (auto index = std::uint32_t{0u}; index < header.entry_point_count; ++index) {
    auto entry_header = entry_record_header{};
    in.read(reinterpret_cast<char*>(&entry_header), sizeof(entry_header));

    if (!in) {
      return std::nullopt;
    }

    auto name = std::string(entry_header.name_length, '\0');
    in.read(name.data(), static_cast<std::streamsize>(entry_header.name_length));

    auto spirv = std::vector<std::uint32_t>(entry_header.spirv_word_count);
    in.read(reinterpret_cast<char*>(spirv.data()), static_cast<std::streamsize>(entry_header.spirv_word_count * sizeof(std::uint32_t)));

    if (!in) {
      return std::nullopt;
    }

    results.push_back(shader_binary_entry{static_cast<VkShaderStageFlagBits>(entry_header.stage), std::move(name), std::move(spirv)});
  }

  return results;
}

auto shader_disk_cache::store(const std::string& key, const std::filesystem::path& source, const std::vector<shader_binary_entry>& compiled) const -> void {
  const auto path = cache_path(key, source);

  auto error = std::error_code{};
  std::filesystem::create_directories(path.parent_path(), error);

  auto out = std::ofstream{path, std::ios::binary};

  if (!out) {
    utility::logger<"graphics">::warn("Shader disk cache: could not write '{}'", path.generic_string());
    return;
  }

  const auto header = shader_cache_header{
    shader_cache_magic,
    shader_cache_format_version,
    static_cast<std::uint32_t>(key.size()),
    static_cast<std::uint32_t>(compiled.size())
  };

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(key.data(), static_cast<std::streamsize>(key.size()));

  for (const auto& entry : compiled) {
    const auto entry_header = entry_record_header{
      static_cast<std::uint32_t>(entry.stage),
      static_cast<std::uint32_t>(entry.name.size()),
      static_cast<std::uint32_t>(entry.spirv.size())
    };

    out.write(reinterpret_cast<const char*>(&entry_header), sizeof(entry_header));
    out.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
    out.write(reinterpret_cast<const char*>(entry.spirv.data()), static_cast<std::streamsize>(entry.spirv.size() * sizeof(std::uint32_t)));
  }
}

} // namespace sbx::graphics
