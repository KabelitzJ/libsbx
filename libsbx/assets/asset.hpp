// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSET_HPP_
#define LIBSBX_ASSETS_ASSET_HPP_

#include <cinttypes>
#include <filesystem>
#include <string>

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
  skelleton,
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
 */
struct asset_record {
  math::uuid id{math::uuid::nil()};
  asset_type type{asset_type::none};
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
 * Concrete asset types must override type(). The tag is matched against the record's type for safe downcasts in assets_module::get<T>.
 */
class asset_base {

public:

  virtual ~asset_base() = default;

  virtual auto type() const -> asset_type = 0;

}; // class asset_base

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_HPP_
