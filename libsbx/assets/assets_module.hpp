// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

/**
 * @brief Owns the asset database and GPU residency.
 *
 * Every path argument to @ref import, @ref import_directory and the `load_*` overloads is
 * interpreted **relative to the active project's assets directory** (core::engine::project()).
 * Assets are keyed by that project-relative path, which keeps serialized references portable and
 * independent of the working directory. A project is required — asset access asserts one is set.
 */
class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module();

  /**
   * @brief Registers an asset by its project-relative path and returns its stable UUID.
   * @param path Path relative to the active project's assets directory.
   */
  auto import(const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Imports every supported asset under a project-relative subdirectory.
   * @param root Subdirectory relative to the assets directory. Empty (default) scans the whole tree.
   */
  auto import_directory(const std::filesystem::path& root = {}) -> void;

  /**
   * @brief Loads a texture from a UUID or project-relative path. If already loaded, returns the existing handle.
   *
   * @param id The UUID of the texture to load.
   * @param format The format to load the texture as. Defaults to sRGB.
   *
   * @return A handle to the loaded texture. Valid
   */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  /**
   * @brief Loads a mesh from a UUID or project-relative path. If already loaded, returns the existing handle.
   *
   * @param id The UUID of the mesh to load.
   * @param path The path (relative to the assets directory) to the mesh file to load.
   *
   * @return A handle to the loaded mesh. Valid if the mesh was successfully loaded.
   */
  auto load_mesh(const math::uuid& id) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path) -> mesh_handle;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Writes a material to a `.material` file.
   * @param path Destination path relative to the active project's assets directory.
   */
  auto save_material(const material_handle& material, const std::filesystem::path& path) -> void;

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

  /** @brief The project-relative path an asset was imported from, or empty if unknown. */
  [[nodiscard]] auto path_of(const math::uuid& id) const -> std::filesystem::path;

private:

  inline static constexpr auto material_capacity = std::uint32_t{1024u};

  struct cooked_submesh {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    math::volume bounds;
    math::uuid material;
  }; // struct cooked_submesh

  struct material_description {
    std::string name{"material"};
    math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    math::vector3 emissive_factor{0.0f, 0.0f, 0.0f};
    std::float_t metallic_factor{1.0f};
    std::float_t roughness_factor{1.0f};
    math::uuid albedo{math::uuid::nil()};
    math::uuid normal{math::uuid::nil()};
    math::uuid metallic_roughness{math::uuid::nil()};
    math::uuid occlusion{math::uuid::nil()};
    math::uuid emissive{math::uuid::nil()};
  }; // struct material_description

  // One row of the asset manifest: the durable uuid -> source path index plus the staleness data
  // (content hash + cooker version) recorded at the last cook.
  struct manifest_entry {
    std::filesystem::path path;        // project source path (same form as _paths)
    std::uint32_t cooker_version{0u};  // cooker that produced the current cooked output (0 = never)
    std::uint64_t source_hash{0u};     // source content hash at last cook
    std::int64_t source_mtime{0};      // source mtime at last cook (fast-path skip)
  }; // struct manifest_entry

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

  auto _register_material(std::shared_ptr<material> record) -> material_handle;

  [[nodiscard]] static auto _absolute(const std::filesystem::path& relative) -> std::filesystem::path;

  [[nodiscard]] auto _cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path;

  // -- Staleness (P3: content hash + cooker version, tracked in the manifest) --

  [[nodiscard]] auto _is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool;

  auto _record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void;

  // -- Asset manifest (P3: uuid -> source path + staleness, so startup needs no rescan) --

  auto _ensure_manifest_loaded() -> void;

  auto _load_manifest() -> void;

  auto _save_manifest() -> void;

  [[nodiscard]] auto _manifest_path() const -> std::filesystem::path;

  auto _cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  auto _load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  [[nodiscard]] static auto _derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  auto _cook_mesh(const std::filesystem::path& source, const std::filesystem::path& relative_source, const math::uuid& id, const std::filesystem::path& cooked) -> bool;

  auto _load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds) -> bool;

  auto _cook_material(const math::uuid& id, const material_description& description) -> bool;

  auto _load_cooked_material(const std::filesystem::path& cooked, const math::uuid& id) -> material_handle;

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, math::uuid> _uuids{};
  std::unordered_map<math::uuid, std::filesystem::path> _paths{}; // project-relative (to assets directory)

  std::unordered_map<math::uuid, manifest_entry> _manifest{};
  bool _manifest_loaded{false};
  bool _manifest_dirty{false};

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
  std::unordered_map<math::uuid, std::shared_ptr<material>> _material_files{};

  texture_handle _white{};
  texture_handle _normal{};
  texture_handle _black{};
  texture_handle _magenta{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
