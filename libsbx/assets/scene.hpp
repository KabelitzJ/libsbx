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

/** @brief A saved scene in scene_serializer's YAML shape. Opaque here, since assets never depends on scenes; scene_serializer does the scene-aware work. */
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
