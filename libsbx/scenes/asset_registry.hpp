// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_ASSET_REGISTRY_HPP_
#define LIBSBX_SCENES_ASSET_REGISTRY_HPP_

#include <unordered_map>
#include <filesystem>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/cube_image.hpp>

namespace sbx::scenes {

struct asset_metadata {
  std::filesystem::path path;
  std::string name;
  std::string type{"unknown"};
  std::string source{"dynamic"};
}; // struct asset_metadata

template<typename Handle>
class asset_table {

public:

  auto has(const utility::hashed_string& name) const -> bool {
    return _by_name.contains(name);
  }

  auto get(const utility::hashed_string& name) const -> const Handle& {
    if (auto entry = _by_name.find(name); entry != _by_name.end()) {
      return entry->second;
    }

    throw utility::runtime_error{"Could not find asset '{}'", name.str()};
  }

  auto insert(const utility::hashed_string& name, const Handle& handle, asset_metadata metadata) -> bool {
    if (_by_name.contains(name)) {
      return false;
    }

    _by_name.emplace(name, handle);
    _metadata.emplace(handle, std::move(metadata));

    return true;
  }

  auto metadata(const Handle& handle) const -> const asset_metadata& {
    return _metadata.at(handle);
  }

  auto size() const -> std::size_t {
    return _by_name.size();
  }

  auto begin() const {
    return _metadata.begin();
  }

  auto end() const {
    return _metadata.end();
  }

  auto begin() {
    return _metadata.begin();
  }

  auto end() {
    return _metadata.end();
  }

private:

  std::unordered_map<utility::hashed_string, Handle> _by_name;
  std::unordered_map<Handle, asset_metadata> _metadata;

}; // class asset_table

class asset_registry {

public:

  template<typename... Args>
  auto request_image(const utility::hashed_string& name, const std::filesystem::path& path, Args&&... args) -> graphics::image2d_handle {
    if (_images.has(name)) {
      return _images.get(name);
    }

    auto& gfx = core::engine::get_module<graphics::graphics_module>();

    const auto handle = gfx.add_resource<graphics::image2d>(path, std::forward<Args>(args)...);

    _images.insert(name, handle, asset_metadata{path, name.str(), "image", "disk"});

    return handle;
  }

  auto has_image(const utility::hashed_string& name) const -> bool {
    return _images.has(name);
  }

  auto get_image(const utility::hashed_string& name) const -> graphics::image2d_handle {
    return _images.get(name);
  }

  auto image_metadata(const graphics::image2d_handle& handle) const -> const asset_metadata& {
    return _images.metadata(handle);
  }

  auto images() const -> const asset_table<graphics::image2d_handle>& {
    return _images;
  }

  template<typename... Args>
  auto request_cube_image(const utility::hashed_string& name, const std::filesystem::path& path, Args&&... args) -> graphics::cube_image2d_handle {
    if (_cube_images.has(name)) {
      return _cube_images.get(name);
    }

    auto& gfx = core::engine::get_module<graphics::graphics_module>();

    const auto handle = gfx.add_resource<graphics::cube_image>(path, std::forward<Args>(args)...);

    _cube_images.insert(name, handle, asset_metadata{path, name.str(), "cube_image", "disk"});

    return handle;
  }

  auto has_cube_image(const utility::hashed_string& name) const -> bool {
    return _cube_images.has(name);
  }

  auto get_cube_image(const utility::hashed_string& name) const -> graphics::cube_image2d_handle {
    return _cube_images.get(name);
  }

  auto cube_image_metadata(const graphics::cube_image2d_handle& handle) const -> const asset_metadata& {
    return _cube_images.metadata(handle);
  }

  auto cube_images() const -> const asset_table<graphics::cube_image2d_handle>& {
    return _cube_images;
  }

  template<typename Mesh, typename... Args>
  auto request_mesh(const utility::hashed_string& name, Args&&... args) -> math::uuid {
    if (_meshes.has(name)) {
      return _meshes.get(name);
    }

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    const auto id = assets_module.add_asset<Mesh>(std::forward<Args>(args)...);

    _meshes.insert(name, id, asset_metadata{"", name.str(), "mesh", "generated"});

    return id;
  }

  template<typename Mesh, typename Path, typename... Args>
  requires (std::is_constructible_v<std::filesystem::path, Path>)
  auto request_mesh(const utility::hashed_string& name, const Path& path, Args&&... args) -> math::uuid {
    if (_meshes.has(name)) {
      return _meshes.get(name);
    }

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    const auto id = assets_module.add_asset<Mesh>(path, std::forward<Args>(args)...);

    _meshes.insert(name, id, asset_metadata{path, name.str(), "mesh", "disk"});

    return id;
  }

  auto has_mesh(const utility::hashed_string& name) const -> bool {
    return _meshes.has(name);
  }

  auto get_mesh(const utility::hashed_string& name) const -> math::uuid {
    return _meshes.get(name);
  }

  auto mesh_metadata(const math::uuid& handle) const -> const asset_metadata& {
    return _meshes.metadata(handle);
  }

  auto meshes() const -> const asset_table<math::uuid>& {
    return _meshes;
  }

  template<typename Animation, typename... Args>
  auto request_animation(const utility::hashed_string& name, Args&&... args) -> math::uuid {
    if (_animations.has(name)) {
      return _animations.get(name);
    }

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    const auto id = assets_module.add_asset<Animation>(std::forward<Args>(args)...);

    _animations.insert(name, id, asset_metadata{"", name.str(), "animation", "generated"});

    return id;
  }

  auto has_animation(const utility::hashed_string& name) const -> bool {
    return _animations.has(name);
  }

  auto get_animation(const utility::hashed_string& name) const -> math::uuid {
    return _animations.get(name);
  }

  auto animations() const -> const asset_table<math::uuid>& {
    return _animations;
  }

  template<typename Material, typename... Args>
  auto request_material(const utility::hashed_string& name, Args&&... args) -> Material& {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    if (_materials.has(name)) {
      return assets_module.get_asset<Material>(_materials.get(name));
    }

    const auto id = assets_module.add_asset<Material>(std::forward<Args>(args)...);

    _materials.insert(name, id, asset_metadata{"", name.str(), "material", "dynamic"});

    return assets_module.get_asset<Material>(id);
  }

  auto has_material(const utility::hashed_string& name) const -> bool {
    return _materials.has(name);
  }

  auto get_material(const utility::hashed_string& name) const -> math::uuid {
    return _materials.get(name);
  }

  auto material_metadata(const math::uuid& handle) const -> const asset_metadata& {
    return _materials.metadata(handle);
  }

  auto materials() const -> const asset_table<math::uuid>& {
    return _materials;
  }

  auto rebase_paths(std::string_view old_uri, std::string_view new_uri) -> void {
    _rebase_table(_images, old_uri, new_uri);
    _rebase_table(_cube_images, old_uri, new_uri);
    _rebase_table(_meshes, old_uri, new_uri);
    _rebase_table(_animations, old_uri, new_uri);
    _rebase_table(_materials, old_uri, new_uri);
  }

  auto clear_paths_under(std::string_view uri) -> void {
    _clear_table_paths_under(_images, uri);
    _clear_table_paths_under(_cube_images, uri);
    _clear_table_paths_under(_meshes, uri);
    _clear_table_paths_under(_animations, uri);
    _clear_table_paths_under(_materials, uri);
  }

private:

  template<typename Table>
  static auto _rebase_table(Table& table, std::string_view old_uri, std::string_view new_uri) -> void {
    for (auto& [handle, metadata] : table) {
      static_cast<void>(handle);

      if (metadata.path.empty()) {
        continue;
      }

      const auto current = metadata.path.generic_string();

      if (!_uri_starts_with(current, old_uri)) {
        continue;
      }

      auto rebuilt = std::string{new_uri};
      rebuilt.append(current, old_uri.size(), std::string::npos);

      metadata.path = std::filesystem::path{rebuilt};
    }
  }

  template<typename Table>
  static auto _clear_table_paths_under(Table& table, std::string_view uri) -> void {
    for (auto& [handle, metadata] : table) {
      static_cast<void>(handle);

      if (metadata.path.empty()) {
        continue;
      }

      const auto current = metadata.path.generic_string();

      if (!_uri_starts_with(current, uri)) {
        continue;
      }

      metadata.path.clear();
    }
  }

  static auto _uri_starts_with(std::string_view path, std::string_view prefix) -> bool {
    if (path.size() < prefix.size()) {
      return false;
    }

    if (path.substr(0, prefix.size()) != prefix) {
      return false;
    }

    if (path.size() == prefix.size()) {
      return true;
    }

    return path[prefix.size()] == '/';
  }

  asset_table<graphics::image2d_handle> _images;
  asset_table<graphics::cube_image2d_handle> _cube_images;
  asset_table<math::uuid> _meshes;
  asset_table<math::uuid> _animations;
  asset_table<math::uuid> _materials;

}; // class asset_registry

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_ASSET_REGISTRY_HPP_
