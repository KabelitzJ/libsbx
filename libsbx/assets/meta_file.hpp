// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_META_FILE_HPP_
#define LIBSBX_ASSETS_META_FILE_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

/**
 * @brief Parsed contents of an .meta sidecar file.
 *
 * The import_settings node is opaque to the database; each importer interprets it according to the asset type.
 */
struct meta_data {
  math::uuid id{math::uuid::nil()};
  std::string type{};
  YAML::Node import_settings{};
}; // struct meta_data

/**
 * @brief Returns the .meta path for a given asset source.
 *
 * Example: `res://textures/base.png` -> `res://textures/base.png.meta`.
 */
auto meta_path_for(const std::filesystem::path& source) -> std::filesystem::path;

/**
 * @brief Reads a .meta sidecar. Returns nullopt if the file does not exist or is malformed.
 */
auto read_meta_file(const std::filesystem::path& meta_path) -> std::optional<meta_data>;

/**
 * @brief Writes a .meta sidecar. Overwrites any existing file at the path.
 *
 * @throws sbx::utility::runtime_error if the file cannot be opened for writing.
 */
auto write_meta_file(const std::filesystem::path& meta_path, const meta_data& data) -> void;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_META_FILE_HPP_
