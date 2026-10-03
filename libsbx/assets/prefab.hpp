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

/** @brief A saved node subtree template in scene_serializer's YAML shape. Opaque here, since assets never depends on scenes; scene_serializer does the scene-aware work. */
class prefab final : public loadable {

  friend class assets_module;

public:

  prefab() = default;

  /**
   * @brief A prefab built in memory without assets_module, e.g. in tests.
   *
   * @param snapshot The subtree snapshot.
   * @param id The prefab's uuid.
   * @param name The prefab's name.
   */
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
