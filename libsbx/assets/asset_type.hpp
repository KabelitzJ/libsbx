// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_TYPE_HPP_
#define LIBSBX_ASSETS_ASSET_TYPE_HPP_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>

#include <libsbx/reflection/enum.hpp>

namespace sbx::assets {

/** @brief An asset file's type, inferred from its extension. */
enum class [[=reflection::named]] asset_type : std::uint8_t {
  unknown,
  texture,
  mesh,
  material,
  environment_map,
  particle_effect,
  animation_graph,
  shader_graph,
  font,
  audio_clip,
  prefab,
  scene, // no cook step, but manifest-registered like prefab
  script // reference-only, compiled by scripting::script_compiler
}; // enum class asset_type

/** @brief The single extension -> type table; case-insensitive. */
[[nodiscard]] auto asset_type_of(const std::filesystem::path& path) -> asset_type;

/** @brief Every (extension, type) pair asset_type_of knows; extensions are lowercase with the leading dot. */
[[nodiscard]] auto known_extensions() -> std::span<const std::pair<std::string_view, asset_type>>;

/** @brief Whether the manifest tracks files of this type (everything but unknown and script). */
[[nodiscard]] auto is_manifested(asset_type type) -> bool;

/** @brief Whether the type has an importer with settings stored in its `.meta`. */
[[nodiscard]] auto has_import_settings(asset_type type) -> bool;

/** @brief A short human-readable name, e.g. "Environment Map". */
[[nodiscard]] auto display_name(asset_type type) -> std::string_view;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_TYPE_HPP_
