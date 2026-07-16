// SPDX-License-Identifier: MIT
#include <libsbx/animations/skinned_mesh_serializer.hpp>

#include <libsbx/animations/mesh.hpp>

namespace sbx::animations {

auto skinned_mesh_serializer::type() const -> std::string_view {
  return sub_id_name;
}

auto skinned_mesh_serializer::enumerate(const assets::serializer_context& context) -> std::vector<assets::sub_asset_info> {
  static_cast<void>(context);

  return {assets::sub_asset_info{.sub_id = std::string{sub_id_name}, .type = std::string{sub_id_name}}};
}

auto skinned_mesh_serializer::owns(const assets::serializer_context& context, std::string_view sub_id) -> bool {
  static_cast<void>(context);

  return sub_id == sub_id_name;
}

auto skinned_mesh_serializer::read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset> {
  return std::make_unique<animations::mesh>(context.source);
}

auto skinned_mesh_serializer::write(const assets::serializer_context& context, const std::unique_ptr<assets::asset>& asset) -> bool {
  static_cast<void>(context);
  static_cast<void>(asset);

  return false;
}

} // namespace sbx::animations
