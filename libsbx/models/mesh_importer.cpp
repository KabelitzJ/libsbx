// SPDX-License-Identifier: MIT
#include <libsbx/models/mesh_importer.hpp>

#include <libsbx/assets/importer_registry.hpp>

namespace sbx::models {

auto mesh_importer::type() const -> std::string_view {
  return models::mesh::type_name;
}

auto mesh_importer::import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> {
  auto settings = context.settings;

  const auto lod_count = settings["lod_count"].as<std::uint32_t>(1u);

  return std::make_unique<models::mesh>(context.resolved, lod_count);
}

} // namespace sbx::models
