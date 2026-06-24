// SPDX-License-Identifier: MIT
#include <libsbx/models/mesh_importer.hpp>

#include <libsbx/assets/serializer_registry.hpp>

namespace sbx::models {

auto mesh_importer::type() const -> std::string_view {
  return models::mesh::type_name;
}

auto mesh_importer::read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset_base> {
  auto settings = context.settings;

  const auto lod_count = settings["lod_count"].as<std::uint32_t>(1u);

  return std::make_unique<models::mesh>(context.resolved, lod_count);
}

auto mesh_importer::write(const assets::serializer_context& context, const std::unique_ptr<assets::asset_base>& asset) -> bool {
  return true;
}

} // namespace sbx::models
