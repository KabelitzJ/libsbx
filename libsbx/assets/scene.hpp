// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SCENE_HPP_
#define LIBSBX_ASSETS_SCENE_HPP_

#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A saved scene ("level") — the same YAML shape scenes::scene_serializer::build produces,
 * opaque to this module: assets deliberately knows nothing about scenes/nodes/components (scenes
 * depends on assets, never the other way), so every scene-aware piece (building this from a live
 * scenes::scene, applying it back) lives in scenes::scene_serializer instead. This class and
 * assets_module just keep the payload alive, versioned (loadable::generation()), and persisted.
 */
class scene final : public loadable {

  friend class assets_module;

public:

  scene() = default;

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
  std::string _name{"Scene"};

}; // class scene

using scene_handle = asset_handle<scene>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SCENE_HPP_
