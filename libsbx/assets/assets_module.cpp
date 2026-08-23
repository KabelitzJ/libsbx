// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/assets_module.hpp>

namespace sbx::assets {

assets_module::assets_module()
: _residency{_cooker, _ibl} { }

auto assets_module::import(const std::filesystem::path& path) -> math::uuid {
  return _cooker.import(path);
}

auto assets_module::import_directory(const std::filesystem::path& root) -> void {
  _cooker.import_directory(root);
}

auto assets_module::load_texture(const math::uuid& id, graphics::format format) -> texture_handle {
  return _residency.load_texture(id, format);
}

auto assets_module::load_texture(const std::filesystem::path& path, graphics::format format) -> texture_handle {
  return _residency.load_texture(path, format);
}

auto assets_module::load_mesh(const math::uuid& id, const mesh_import_options& options) -> mesh_handle {
  return _residency.load_mesh(id, options);
}

auto assets_module::load_mesh(const std::filesystem::path& path, const mesh_import_options& options) -> mesh_handle {
  return _residency.load_mesh(path, options);
}

auto assets_module::load_material(const math::uuid& id) -> material_handle {
  return _residency.load_material(id);
}

auto assets_module::load_material(const std::filesystem::path& path) -> material_handle {
  return _residency.load_material(path);
}

auto assets_module::create_material(const material::create_info& create_info) -> material_handle {
  return _residency.create_material(create_info);
}

auto assets_module::update_material(material_handle& material, const material::create_info& create_info) -> void {
  _residency.update_material(material, create_info);
}

auto assets_module::save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid {
  return _residency.save_material(material, path);
}

auto assets_module::load_environment_map(const math::uuid& id) -> environment_map_handle {
  return _residency.load_environment_map(id);
}

auto assets_module::load_environment_map(const std::filesystem::path& path) -> environment_map_handle {
  return _residency.load_environment_map(path);
}

auto assets_module::process_uploads(std::uint64_t frame_index) -> void {
  _residency.process_uploads(frame_index);
}

auto assets_module::is_resident(const texture_handle& texture) const -> bool {
  return _residency.is_resident(texture);
}

auto assets_module::is_resident(const mesh_handle& mesh) const -> bool {
  return _residency.is_resident(mesh);
}

auto assets_module::is_resident(const material_handle& material) const -> bool {
  return _residency.is_resident(material);
}

auto assets_module::is_resident(const environment_map_handle& environment) const -> bool {
  return _residency.is_resident(environment);
}

auto assets_module::path_of(const math::uuid& id) const -> std::filesystem::path {
  return _cooker.path_of(id);
}

} // namespace sbx::assets
