// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_PREFAB_HPP_
#define LIBSBX_ASSETS_PREFAB_HPP_

#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A saved node subtree template ("blueprint") — the same YAML shape
 * scenes::scene_serializer::serialize_subtree produces, opaque to this module: assets deliberately
 * knows nothing about scenes/nodes/components (scenes depends on assets, never the other way), so
 * every scene-aware piece (turning a subtree into this, instantiating it back, per-component
 * merging) lives in scenes::scene_serializer instead. This class and assets_module just keep the
 * payload alive, versioned (loadable::generation()), and persisted.
 */
class prefab final : public loadable {

  friend class assets_module;

public:

  prefab() = default;

  /** @brief A prefab built in memory, not loaded or registered through assets_module (e.g. in tests). */
  prefab(YAML::Node snapshot, const math::uuid& id, std::string name)
  : _snapshot{std::move(snapshot)},
    _id{id},
    _name{std::move(name)} { }

  [[nodiscard]] auto snapshot() const noexcept -> const YAML::Node& {
    return _snapshot;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  YAML::Node _snapshot{};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"Prefab"};

}; // class prefab

using prefab_handle = asset_handle<prefab>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_PREFAB_HPP_
