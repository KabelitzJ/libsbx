// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MESH_IMPORTER_HPP_
#define LIBSBX_MODELS_MESH_IMPORTER_HPP_

#include <cstdint>
#include <memory>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/importer.hpp>

#include <libsbx/models/mesh.hpp>

namespace sbx::models {

/**
 * @brief Imports a mesh source file (`.gltf`, cooked to `.sbxstmsh` on first load) into a mesh asset.
 *
 * Reads `lod_count` (default 1) from the asset's .meta `import_settings`.
 */
class mesh_importer final : public assets::importer {

public:

  auto type() const -> std::string_view override;

  auto import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> override;

}; // class mesh_importer

} // namespace sbx::models

#endif // LIBSBX_MODELS_MESH_IMPORTER_HPP_
