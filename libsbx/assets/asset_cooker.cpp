// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::assets {

auto asset_cooker::cooked_path(const math::uuid& id, std::string_view extension) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.library_directory() / fmt::format("{}{}", id.value(), extension);
}


} // namespace sbx::assets
