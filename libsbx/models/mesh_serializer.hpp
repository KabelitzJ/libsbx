// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MESH_SERIALIZER_HPP_
#define LIBSBX_MODELS_MESH_SERIALIZER_HPP_

#include <cstdint>
#include <memory>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/serializer_registry.hpp>

#include <libsbx/models/mesh.hpp>

namespace sbx::models {

/**
 * @brief Imports a mesh source file (`.gltf`, cooked to `.sbxstmsh` on first load) into a mesh asset.
 *
 * Reads `lod_count` (default 1) from the asset's .meta `import_settings`.
 */
class mesh_serializer final : public assets::serializer<mesh_serializer> {

  inline static const auto is_registered = register_serializer({".gltf", ".sbxstmsh"});

public:

  auto type() const -> std::string_view override;

  auto read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset> override;

  auto write(const assets::serializer_context& context, const std::unique_ptr<assets::asset>& asset) -> bool override;

}; // class mesh_serializer

} // namespace sbx::models

#endif // LIBSBX_MODELS_MESH_SERIALIZER_HPP_
