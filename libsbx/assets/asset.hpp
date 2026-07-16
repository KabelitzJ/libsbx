// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSET_HPP_
#define LIBSBX_ASSETS_ASSET_HPP_

#include <cinttypes>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

enum class load_state : std::uint8_t {
  unloaded,
  loading,
  ready,
  failed
}; // enum class load_state

enum class asset_type : std::uint8_t {
  none = 0,
  texture,
  environment_map,
  material,
  static_mesh,
  skinned_mesh,
  skeleton,
  animation,
  audio,
  font,
  script
}; // enum class asset_type

/**
 * @brief Canonical metadata for an asset, keyed by its UUID.
 *
 * The record never owns the payload (mesh/texture/material data). It owns the identity (UUID, type tag, source path) and the lifecycle state.
 * The typed payload is stored separately by the assets_module.
 *
 * The type tag is a string that matches the producing importer's type() and the .meta `type` field.
 *
 * A single source file may back multiple records (e.g. a `.gltf` yielding a static mesh, a skeleton and several animations).
 * In that case each record shares the same @ref source but carries a distinct @ref sub_id. The record whose sub_id is empty is the primary asset of the source; @ref parent points every sub-asset back to that primary (nil for the primary itself).
 */
struct asset_record {
  math::uuid id{math::uuid::nil()};
  math::uuid parent{math::uuid::nil()};
  std::string type{};
  std::string sub_id{};
  std::filesystem::path source{};
  std::uint32_t generation{0u};
  std::uint32_t reference_count{0u};
  load_state state{load_state::unloaded};
}; // struct asset_record

/**
 * @brief Polymorphic base for every typed asset payload owned by the assets_module.
 *
 * Asset types do not store identity (UUID, source path) themselves; that lives in the asset_record.
 * The payload only carries the data that the engine uses at runtime (e.g. a graphics handle wrapper for a texture).
 *
 * Concrete asset types must override type(). Composite assets (a material referencing textures, a scene referencing everything) override dependencies() to return the UUIDs they hold a strong reference to; the assets_module releases those when the asset is released.
 */
class asset {

public:

  virtual ~asset() = default;

  virtual auto type() const -> asset_type = 0;

  virtual auto dependencies() const -> std::span<const math::uuid> {
    return {};
  }

}; // class asset

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_HPP_
