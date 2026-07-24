// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>

namespace sbx::assets {

class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module();

  auto import(const std::filesystem::path& path) -> math::uuid;

  auto import_directory(const std::filesystem::path& root) -> void;

  /**
   * @brief Loads a texture from a UUID or path. If the texture has already been loaded, returns the existing handle.
   * 
   * @param id The UUID of the texture to load.
   * @param format The format to load the texture as. Defaults to sRGB.
   *
   * @return A handle to the loaded texture. Valid
   */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  /**
   * @brief Loads a mesh from a UUID or path. If the mesh has already been loaded, returns the existing handle.
   * 
   * @param id The UUID of the mesh to load.
   * @param path The path to the mesh file to load.
   * 
   * @return A handle to the loaded mesh. Valid if the mesh was successfully loaded.
   */
  auto load_mesh(const math::uuid& id) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path) -> mesh_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Render thread. Turns queued texture loads into GPU images + bindless writes. Copies are
   * recorded by the caller's subsequent upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto white_texture() const noexcept -> texture_handle {
    return _white;
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture_handle {
    return _normal;
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture_handle {
    return _black;
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture_handle {
    return _magenta;
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _material_address;
  }

private:

  inline static constexpr auto material_capacity = std::uint32_t{1024u};

  struct pending_texture_upload {
    std::uint32_t index;
    std::vector<std::byte> pixels;
    std::uint32_t width;
    std::uint32_t height;
    graphics::format format;
  }; // struct pending_texture_upload

  struct pending_mesh_upload {
    std::shared_ptr<mesh> record;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
  }; // struct pending_mesh_upload

  struct pending_material_upload {
    std::shared_ptr<material> record;
  }; // struct pending_material_upload

  auto _create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle;

  auto _read_or_create_meta(const std::filesystem::path& path) -> math::uuid;

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, math::uuid> _uuids{};
  std::unordered_map<math::uuid, std::filesystem::path> _paths{};

  std::unordered_map<std::string, std::shared_ptr<texture>> _textures{};
  std::vector<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  std::unordered_map<math::uuid, std::shared_ptr<mesh>> _meshes{};
  std::vector<pending_mesh_upload> _pending_meshes{};

  std::vector<std::shared_ptr<material>> _materials{};
  std::vector<pending_material_upload> _pending_materials{};
  graphics::buffer_handle _material_buffer{};
  graphics::buffer::address_type _material_address{0u};
  std::uint32_t _material_count{0u};

  texture_handle _white{};
  texture_handle _normal{};
  texture_handle _black{};
  texture_handle _magenta{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
