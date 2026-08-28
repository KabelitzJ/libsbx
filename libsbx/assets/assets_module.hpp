// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <cstdint>
#include <filesystem>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/asset_residency.hpp>
#include <libsbx/assets/ibl_baker.hpp>

namespace sbx::assets {

/**
 * @brief Owns the asset database and GPU residency; a thin facade over @ref asset_cooker
 * (import/cook/manifest), @ref asset_residency (GPU-resident uploads), and @ref ibl_baker
 * (IBL cubemap bakes) — kept as a single registered module with an identical public surface.
 *
 * `load_*` path overloads resolve relative to the active project's assets directory. @ref import
 * and @ref import_directory do not: their path/root must already be resolvable from the process's
 * cwd (typically `project.assets_directory() / relative`) — passing a bare assets-relative path
 * directly to them silently mints a second, broken uuid rather than failing loudly. A project is
 * required — asset access asserts one is set.
 */
class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module() = default;

  /**
   * @brief Registers an asset by its path and returns its stable UUID.
   * @param path Must be resolvable from the current working directory (see class doc comment) —
   * not merely relative to the assets directory. Prefer a `load_*(path)` overload where possible;
   * it resolves for you.
   */
  auto import(const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Imports every supported asset under a subdirectory.
   * @param root Must be resolvable from the current working directory (see class doc comment).
   * Empty (default) is *not* "the whole assets tree" here — pass `project.assets_directory()`
   * explicitly for that.
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
   * @param options Only consulted the first time this mesh is actually cooked (a cache hit ignores
   * it) — see @ref mesh_import_options.
   *
   * @return A handle to the loaded mesh. Valid if the mesh was successfully loaded.
   */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}) -> mesh_handle;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites an existing material's fields in place; every material_handle already
   * pointing at it (shared object) observes the change immediately. Does not touch identity
   * (index/uuid) or persist to disk — pair with save_material to do that.
   */
  auto update_material(material_handle& material, const material::create_info& create_info) -> void;

  /**
   * @brief Writes a material to a `.material` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The material's canonical uuid (also written back onto the material record itself).
   */
  auto save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid;

  auto load_environment_map(const math::uuid& id) -> environment_map_handle;

  auto load_environment_map(const std::filesystem::path& path) -> environment_map_handle;

  auto load_particle_effect(const math::uuid& id) -> particle_effect_handle;

  auto load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle;

  auto create_particle_effect(const particle_effect::create_info& create_info) -> particle_effect_handle;

  auto update_particle_effect(particle_effect_handle& effect, const particle_effect::create_info& create_info) -> void;

  auto save_particle_effect(particle_effect_handle& effect, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Render thread. Turns queued texture loads into GPU images + bindless writes. Copies are
   * recorded by the caller's subsequent upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  [[nodiscard]] auto white_texture() const noexcept -> texture_handle {
    return _residency.white_texture();
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture_handle {
    return _residency.normal_texture();
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture_handle {
    return _residency.black_texture();
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture_handle {
    return _residency.magenta_texture();
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _residency.material_buffer_address();
  }

  /**
   * @brief The bindless index of the global BRDF LUT, baked once (lazily, on the first
   * environment-map load) and shared by every environment. `environment_map::invalid_index` if
   * nothing has been loaded yet.
   */
  [[nodiscard]] auto brdf_lut_index() const noexcept -> std::uint32_t {
    return _ibl.brdf_lut_index();
  }

  /** @brief The project-relative path an asset was imported from, or empty if unknown. */
  [[nodiscard]] auto path_of(const math::uuid& id) const -> std::filesystem::path;

private:

  // Declaration order is construction order: residency depends on the other two so it's declared
  // last, and destroyed first — the one still holding live GPU-facing state.
  asset_cooker _cooker{};
  ibl_baker _ibl{};
  asset_residency _residency;

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
