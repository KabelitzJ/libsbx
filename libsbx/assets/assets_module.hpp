// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/font.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/shader_graph.hpp>
#include <libsbx/assets/prefab.hpp>
#include <libsbx/assets/scene.hpp>
#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/asset_manifest.hpp>
#include <libsbx/assets/asset_residency.hpp>
#include <libsbx/assets/ibl_baker.hpp>

namespace sbx::assets {

/**
 * @brief Owns the asset database and GPU residency; a thin facade over @ref asset_cooker
 * (import/cook/manifest), @ref asset_residency (GPU-resident uploads), and @ref ibl_baker
 * (IBL cubemap bakes).
 *
 * `load_*` path overloads resolve relative to the assets directory; @ref import and
 * @ref import_directory require an already-cwd-resolvable path instead — passing a bare
 * assets-relative path to them silently mints a second, broken uuid. A project must be set.
 */
class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module() = default;

  /**
   * @brief Registers an asset by its path and returns its stable UUID.
   * @param path Must be resolvable from the current working directory (see class doc comment) —
   * not merely relative to the assets directory. Prefer a `load_*(path)` overload where possible.
   */
  auto import(const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Imports every supported asset under a subdirectory.
   * @param root Must be resolvable from the current working directory. Empty (default) is *not*
   * "the whole assets tree" here — pass `project.assets_directory()` explicitly for that.
   */
  auto import_directory(const std::filesystem::path& root = {}) -> void;

  /**
   * @brief Moves or renames a file or directory (asset-manifested or not — a plain script,
   * scene, or folder works too) on disk, keeping its identity: a manifested file's uuid survives
   * (its `.meta` moves with it) and every `.material` file that referenced a moved texture by
   * path is rewritten to the new path.
   * @param old_path @param new_path Both must be resolvable from the current working directory,
   * same requirement as @ref import.
   * @return false if the underlying filesystem rename failed (e.g. `new_path` already exists).
   */
  auto move_asset(const std::filesystem::path& old_path, const std::filesystem::path& new_path) -> bool;

  /**
   * @brief Deletes a file or directory (asset-manifested or not) from disk, along with its
   * `.meta` sidecar(s), and drops it from the asset manifest. Recurses for a directory.
   */
  auto delete_asset(const std::filesystem::path& path) -> void;

  /**
   * @brief Loads a texture from a UUID or project-relative path; returns the existing handle if already loaded.
   *
   * @param id UUID of the texture to load.
   * @param format Pixel format to load as (defaults to sRGB).
   *
   * @return Handle to the loaded texture.
   */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  /** @brief Loads a TTF -> SDF glyph atlas font from a UUID or project-relative path; returns the existing handle if already loaded. */
  auto load_font(const math::uuid& id) -> font_handle;

  auto load_font(const std::filesystem::path& path) -> font_handle;

  /**
   * @brief Loads a mesh from a UUID or project-relative path; returns the existing handle if already loaded.
   *
   * @param id UUID of the mesh to load.
   * @param options Only consulted the first time this mesh is actually cooked; a cache hit ignores it. See @ref mesh_import_options.
   *
   * @return Handle to the loaded mesh, valid only if it loaded successfully.
   */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  /** @ref asset_residency::create_mesh */
  auto create_mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, std::vector<mesh::submesh> submeshes, const math::volume& bounds) -> mesh_handle;

  /**
   * @brief Resolves a mesh's raw cooked vertex/index data (see @ref cooked_mesh_data), independent
   * of GPU residency — the mesh need not be, and does not become, loaded via @ref load_mesh.
   *
   * Fully synchronous, main-thread only -- bypasses asset_residency/asset_loader entirely, same as
   * it always has. Used by physics::mesh_collision_cache to build a collider's triangle BVH.
   */
  auto resolve_mesh_collision_data(const math::uuid& id) -> std::optional<cooked_mesh_data>;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites an existing material's fields in place; every material_handle already
   * pointing at it observes the change immediately. Does not touch identity (index/uuid) or
   * persist to disk — pair with @ref save_material for that.
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

  auto load_animation_graph(const math::uuid& id) -> animation_graph_handle;

  auto load_animation_graph(const std::filesystem::path& path) -> animation_graph_handle;

  auto create_animation_graph(const animation_graph::create_info& create_info) -> animation_graph_handle;

  auto update_animation_graph(animation_graph_handle& graph, const animation_graph::create_info& create_info) -> void;

  auto save_animation_graph(animation_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  auto load_shader_graph(const math::uuid& id) -> shader_graph_handle;

  auto load_shader_graph(const std::filesystem::path& path) -> shader_graph_handle;

  auto create_shader_graph(const shader_graph::create_info& create_info) -> shader_graph_handle;

  auto update_shader_graph(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  // See asset_residency::update_shader_graph_data's doc comment -- the lightweight, no-recompile
  // sibling of update_shader_graph, for every live edit in the graph editor.
  auto update_shader_graph_data(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  // See asset_residency::update_shader_graph_node_position's doc comment -- the lightweight,
  // no-recompile sibling of update_shader_graph, for a node drag's every-frame position update.
  auto update_shader_graph_node_position(shader_graph_handle& graph, std::uint32_t node_id, math::vector2 position) -> void;

  auto save_shader_graph(shader_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Wraps an already-serialized subtree snapshot (see scenes::scene_serializer::
   * serialize_subtree) as a new, unsaved prefab asset. Scene-agnostic on purpose — see prefab.hpp's
   * doc comment; scenes::scene_serializer::create_prefab_from_node is the scene-aware entry point
   * editor code actually calls.
   */
  auto create_prefab(YAML::Node snapshot, std::string name) -> prefab_handle;

  auto load_prefab(const math::uuid& id) -> prefab_handle;

  auto load_prefab(const std::filesystem::path& path) -> prefab_handle;

  /**
   * @brief Overwrites an existing prefab's snapshot in place; every prefab_handle already pointing
   * at it observes the change immediately (loadable::generation() bumps). Does not persist to disk
   * or touch identity (id) — pair with @ref save_prefab for that.
   */
  auto update_prefab(prefab_handle& prefab, YAML::Node snapshot) -> void;

  /**
   * @brief Writes a prefab to a `.prefab` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The prefab's canonical uuid (also written back onto the prefab record itself).
   */
  auto save_prefab(prefab_handle& prefab, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Wraps an already-built scene snapshot (see scenes::scene_serializer::build) as a new,
   * unsaved scene asset. Scene-agnostic on purpose — same reasoning as create_prefab; the caller
   * builds the YAML::Node via scene_serializer and still needs @ref save_scene to persist it.
   */
  auto create_scene(YAML::Node snapshot, std::string name) -> scene_handle;

  auto load_scene(const math::uuid& id) -> scene_handle;

  auto load_scene(const std::filesystem::path& path) -> scene_handle;

  /**
   * @brief Overwrites an existing scene's snapshot in place; every scene_handle already pointing
   * at it observes the change immediately. Does not persist to disk or touch identity (id) —
   * pair with @ref save_scene for that.
   */
  auto update_scene(scene_handle& scene, YAML::Node snapshot) -> void;

  /**
   * @brief Writes a scene to a `.scene` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The scene's canonical uuid (also written back onto the scene record itself).
   */
  auto save_scene(scene_handle& scene, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Turns queued texture loads into GPU images and bindless writes.
   *
   * Runs on the render thread; copies are recorded by the caller's subsequent @ref upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  [[nodiscard]] auto is_resident(const font_handle& font) const -> bool;

  /** @ref asset_residency::resident_asset_counts */
  [[nodiscard]] auto resident_asset_counts() const -> assets::resident_asset_counts {
    return _residency.resident_asset_counts();
  }

  /** @ref asset_residency::image_view_of */
  [[nodiscard]] auto image_view_of(const texture_handle& texture) const -> VkImageView {
    return _residency.image_view_of(texture);
  }

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

  /**
   * @brief Rewrites every `.material` file under the assets directory whose texture slots point
   * at `old_relative` (assets-relative, as persisted by @ref save_material) to point at
   * `new_relative` instead — the counterpart to @ref move_asset moving the texture file itself.
   * Only called for extensions @ref move_asset recognizes as a texture; materials are the only
   * asset kind referencing another asset by path rather than uuid.
   */
  auto _fixup_material_texture_references(const std::filesystem::path& old_relative, const std::filesystem::path& new_relative) -> void;

  asset_manifest _manifest{};
  ibl_baker _ibl{};
  asset_residency _residency;

  // Uuid -> record cache for prefabs loaded by id, same role as asset_residency's *_files maps play
  // for material/particle_effect/animation_graph -- prefab has no GPU state of its own (see
  // prefab.hpp), so it skips asset_residency/asset_loader entirely and lives directly here.
  std::unordered_map<math::uuid, prefab_handle> _prefabs{};

  // Same role as _prefabs above, for scenes -- no GPU state of its own either.
  std::unordered_map<math::uuid, scene_handle> _scenes{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
