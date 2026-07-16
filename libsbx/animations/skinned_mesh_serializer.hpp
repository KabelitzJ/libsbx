// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SKINNED_MESH_SERIALIZER_HPP_
#define LIBSBX_ANIMATIONS_SKINNED_MESH_SERIALIZER_HPP_

#include <memory>
#include <string_view>
#include <vector>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/serializer_registry.hpp>

namespace sbx::animations {

/**
 * @brief Extracts the skinned mesh (geometry plus skeleton) from a container source (`.gltf`).
 *
 * Shares the `.gltf` extension with the static mesh serializer: this one claims only the `skinned_mesh` sub-asset, addressed as `res://model.gltf#skinned_mesh`.
 */
class skinned_mesh_serializer final : public assets::serializer<skinned_mesh_serializer> {

  inline static const auto is_registered = register_serializer({".gltf"});

  inline static constexpr auto sub_id_name = std::string_view{"skinned_mesh"};

public:

  auto type() const -> std::string_view override;

  auto enumerate(const assets::serializer_context& context) -> std::vector<assets::sub_asset_info> override;

  auto owns(const assets::serializer_context& context, std::string_view sub_id) -> bool override;

  auto read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset> override;

  auto write(const assets::serializer_context& context, const std::unique_ptr<assets::asset>& asset) -> bool override;

}; // class skinned_mesh_serializer

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_SKINNED_MESH_SERIALIZER_HPP_
