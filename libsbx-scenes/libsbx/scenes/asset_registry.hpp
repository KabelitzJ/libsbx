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

    auto& am = core::engine::get_module<assets::assets_module>();

    const auto id = am.add_asset<Mesh>(std::forward<Args>(args)...);

    _meshes.insert(name, id, asset_metadata{"", name.str(), "mesh", "generated"});

    return id;
  }

  template<typename Mesh, typename Path, typename... Args>
  requires (std::is_constructible_v<std::filesystem::path, Path>)
  auto request_mesh(const utility::hashed_string& name, const Path& path, Args&&... args) -> math::uuid {
    if (_meshes.has(name)) {
      return _meshes.get(name);
    }

    auto& am = core::engine::get_module<assets::assets_module>();

    const auto id = am.add_asset<Mesh>(path, std::forward<Args>(args)...);

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

    auto& am = core::engine::get_module<assets::assets_module>();

    const auto id = am.add_asset<Animation>(std::forward<Args>(args)...);

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
    auto& am = core::engine::get_module<assets::assets_module>();

    if (_materials.has(name)) {
      return am.get_asset<Material>(_materials.get(name));
    }

    const auto id = am.add_asset<Material>(std::forward<Args>(args)...);

    _materials.insert(name, id, asset_metadata{"", name.str(), "material", "dynamic"});

    return am.get_asset<Material>(id);
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

private:

  asset_table<graphics::image2d_handle> _images;
  asset_table<graphics::cube_image2d_handle> _cube_images;
  asset_table<math::uuid> _meshes;
  asset_table<math::uuid> _animations;
  asset_table<math::uuid> _materials;

}; // class asset_registry

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_ASSET_REGISTRY_HPP_
