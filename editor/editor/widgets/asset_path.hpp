// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_ASSET_PATH_HPP_
#define EDITOR_WIDGETS_ASSET_PATH_HPP_

#include <filesystem>
#include <string>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace editor {

/** @brief An asset's path for display, or "(unknown)". */
inline auto asset_path_text(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::string {
  const auto path = assets_module.path_of(id);
  return path.empty() ? std::string{"(unknown)"} : path.string();
}

/** @brief An asset's path relative to the assets directory, or empty if unknown or outside it. */
inline auto relative_asset_path(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::filesystem::path {
  const auto path = assets_module.path_of(id);

  if (path.empty()) {
    return {};
  }

  auto& project = sbx::core::engine::project();
  const auto relative = std::filesystem::relative(path, project.assets_directory());

  if (relative.empty() || relative.begin()->string() == "..") {
    return {};
  }

  return relative;
}

} // namespace editor

#endif // EDITOR_WIDGETS_ASSET_PATH_HPP_
