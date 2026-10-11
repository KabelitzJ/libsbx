// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_IMPORT_SETTINGS_HPP_
#define LIBSBX_ASSETS_IMPORT_SETTINGS_HPP_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/assets/asset_type.hpp>

namespace sbx::assets {

struct texture_import_settings {
  bool generate_mipmaps{true};

  /** @brief Downscales the longer side to at most this many pixels at cook time; 0 keeps the source size. */
  std::uint32_t max_size{0u};

  bool flip_y{false};

  auto operator==(const texture_import_settings&) const -> bool = default;
}; // struct texture_import_settings

/**
 * @brief Import choices for a mesh.
 *
 * `extract_materials` only affects asset_residency's finalize step; every other field changes the cooked blob.
 */
struct mesh_import_options {
  /** @brief Extracts embedded materials into editable `.material` assets, reusing existing ones. */
  bool extract_materials{true};

  /** @brief Cooks the skin data, skeleton and animation clips; off loads the mesh as unskinned. */
  bool import_skeleton{true};

  /** @brief Which primitives, in mesh_source_summary::primitives order, become submeshes. Empty means all. */
  std::vector<std::size_t> included_primitives{};

  /** @brief Which animation clips to cook, by original index in the source's animation list. Empty means all. */
  std::vector<std::size_t> included_animations{};

  /** @brief Uniform scale baked into positions, joint translations and animation translation keys. */
  std::float_t scale_factor{1.0f};

  bool generate_lods{true};

  auto operator==(const mesh_import_options&) const -> bool = default;
}; // struct mesh_import_options

struct font_import_settings {
  /** @brief The atlas's raster height in pixels; larger is sharper at big sizes but costs atlas memory. */
  std::float_t reference_size{48.0f};

  std::uint32_t first_codepoint{32u};

  /** @brief 224 covers printable ASCII + Latin-1 from the default first codepoint. */
  std::uint32_t codepoint_count{224u};

  auto operator==(const font_import_settings&) const -> bool = default;
}; // struct font_import_settings

enum class [[=reflection::named]] audio_load_type : std::uint8_t {
  decompress_on_load, // decoded to PCM at cook; one shared buffer per clip, best for short sounds
  compressed_in_memory // Ogg Vorbis sources only: kept encoded and decoded while playing, best for music
}; // enum class audio_load_type

struct audio_clip_import_settings {
  /** @brief Ignored for non-Vorbis sources, which always decompress. */
  audio_load_type load_type{audio_load_type::decompress_on_load};

  /** @brief Downmixes to one channel; OpenAL only spatializes mono clips. */
  bool force_mono{false};

  auto operator==(const audio_clip_import_settings&) const -> bool = default;
}; // struct audio_clip_import_settings

/** @brief The `importer:` block of a `.meta`; monostate for types without settings. */
using import_settings = std::variant<std::monostate, texture_import_settings, mesh_import_options, font_import_settings, audio_clip_import_settings>;

[[nodiscard]] auto default_import_settings(asset_type type) -> import_settings;

/** @brief Missing keys fall back to defaults, so older `.meta` files keep working. */
[[nodiscard]] auto decode_import_settings(asset_type type, const YAML::Node& node) -> import_settings;

/** @brief A null node for monostate. */
[[nodiscard]] auto encode_import_settings(const import_settings& settings) -> YAML::Node;

/** @brief Stable across runs; stored with each cook so changed settings force a recook. */
[[nodiscard]] auto import_settings_hash(const import_settings& settings) -> std::uint64_t;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IMPORT_SETTINGS_HPP_
