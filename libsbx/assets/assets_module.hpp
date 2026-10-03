// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture2d.hpp>
#include <libsbx/assets/texture2d_array.hpp>
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
 * @brief Owns the asset database and GPU residency: a facade over asset_manifest, asset_residency and ibl_baker.
 *
 * `load_*` path overloads resolve relative to the assets directory; import, import_directory, move_asset and delete_asset take cwd-resolvable paths instead.
 * Passing a bare assets-relative path to those mints a second, broken uuid. A project must be set.
 */
class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module() = default;

  /**
   * @brief Registers an asset by path.
   *
   * @param path A cwd-resolvable path, not assets-relative. Prefer a `load_*(path)` overload where possible.
   *
   * @return The asset's stable uuid.
   */
  auto import(const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Imports every supported asset under a directory.
   *
   * @param root A cwd-resolvable directory. Empty does not mean the whole assets tree; pass `project.assets_directory()` for that.
   */
  auto import_directory(const std::filesystem::path& root = {}) -> void;

  /**
   * @brief Moves or renames a file or directory, keeping its uuid (the `.meta` moves along) and rewriting `.material` files that referenced a moved texture.
   *
   * @param old_path The cwd-resolvable source path.
   * @param new_path The cwd-resolvable destination path.
   *
   * @return False if the filesystem rename failed, e.g. because `new_path` exists.
   */
  auto move_asset(const std::filesystem::path& old_path, const std::filesystem::path& new_path) -> bool;

  /**
   * @brief Deletes a file or directory with its `.meta` sidecars and drops it from the manifest.
   *
   * @param path The cwd-resolvable path to delete.
   */
  auto delete_asset(const std::filesystem::path& path) -> void;

  /**
   * @brief Loads a texture by uuid or project-relative path, or returns the existing handle.
   *
   * @param id The texture's uuid.
   * @param format The pixel format to load as.
   *
   * @return The texture handle.
   */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture2d_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture2d_handle;

  /**
   * @brief Allocates an empty compute-writable texture.
   *
   * @param width The width in pixels.
   * @param height The height in pixels.
   * @param format The image format.
   *
   * @return The texture handle.
   */
  auto create_storage_image(std::uint32_t width, std::uint32_t height, graphics::format format) -> texture2d_handle;

  /**
   * @brief The GPU image behind a texture's sampled bindless index.
   *
   * @param texture The texture.
   *
   * @return The image, or an empty handle if not resident.
   */
  [[nodiscard]] auto image_handle_for(const texture2d_handle& texture) const -> graphics::image_handle;

  /**
   * @brief Finds a resident texture by uuid under any format.
   *
   * @param id The asset uuid or record handle.
   *
   * @return The texture, or an empty handle.
   */
  [[nodiscard]] auto find_texture(const math::uuid& id) const -> texture2d_handle;

  /**
   * @brief Frees a texture's bindless indices and GPU image.
   *
   * @param texture The texture to free.
   */
  auto release_texture(const texture2d_handle& texture) -> void;

  /**
   * @brief A 2D texture array from resident layers of exactly @p size.
   *
   * @param layers The source textures.
   * @param size The size every layer must have.
   *
   * @return The array, or an invalid handle on failure.
   */
  auto create_texture2d_array(std::span<const texture2d_handle> layers, const math::vector2u& size) -> texture2d_array_handle;

  [[nodiscard]] auto find_texture2d_array(const math::uuid& id) const -> texture2d_array_handle;

  auto release_texture2d_array(const texture2d_array_handle& array) -> void;

  /**
   * @brief Loads a TTF as an SDF glyph atlas font by uuid or project-relative path, or returns the existing handle.
   *
   * @param id The font's uuid.
   *
   * @return The font handle.
   */
  auto load_font(const math::uuid& id) -> font_handle;

  auto load_font(const std::filesystem::path& path) -> font_handle;

  /**
   * @brief Loads a mesh by uuid or project-relative path, or returns the existing handle.
   *
   * @param id The mesh's uuid.
   * @param options Only used when the mesh is actually cooked; ignored on a cache hit.
   * @param force_recook Whether to re-cook even if already loaded.
   *
   * @return The mesh handle.
   */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  /**
   * @brief Builds a mesh from in-memory data, uploaded through the next process_uploads().
   *
   * @param vertices The vertices.
   * @param indices The indices.
   * @param submeshes The submesh ranges and materials.
   * @param bounds The mesh bounds.
   *
   * @return The new mesh.
   */
  auto create_mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, std::vector<mesh::submesh> submeshes, const math::volume& bounds) -> mesh_handle;

  /**
   * @brief Builds a mesh that is resident immediately, for frequently replaced content. Render thread idle only.
   *
   * @param vertices The vertices.
   * @param indices The indices.
   * @param submeshes The submesh ranges and materials.
   * @param bounds The mesh bounds.
   *
   * @return The new mesh.
   */
  auto create_dynamic_mesh(std::span<const vertex> vertices, std::span<const std::uint32_t> indices, std::vector<mesh::submesh> submeshes, const math::volume& bounds) -> mesh_handle;

  /**
   * @brief Retires a mesh's GPU buffers; handles going out of scope never do.
   *
   * @param mesh The mesh to release.
   */
  auto release_mesh(const mesh_handle& mesh) -> void;

  /**
   * @brief Synchronously reads a mesh's cooked vertex/index data without loading it, for collision BVHs. Main thread only.
   *
   * @param id The mesh's uuid.
   *
   * @return The cooked data, or nullopt if it can't be resolved.
   */
  auto resolve_mesh_collision_data(const math::uuid& id) -> std::optional<cooked_mesh_data>;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites a material's fields in place; every handle sees the change. Pair with save_material to persist.
   *
   * @param material The material to update.
   * @param create_info The new fields.
   */
  auto update_material(material_handle& material, const material::create_info& create_info) -> void;

  /**
   * @brief Copies @p source into a new, independently registered material.
   *
   * @param source The material to copy.
   *
   * @return The new material.
   */
  auto duplicate_material(const material_handle& source) -> material_handle;

  /**
   * @brief Frees a material instance's slot for reuse.
   *
   * @param material The material to release.
   */
  auto release_material(const material_handle& material) -> void;

  /**
   * @brief Writes a material to a `.material` file and registers it as an asset.
   *
   * @param material The material to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The material's canonical uuid.
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

  // Cheap update without re-cooking, for live graph editor edits.
  auto update_shader_graph_data(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  // Cheap node move without re-cooking, for node drags.
  auto update_shader_graph_node_position(shader_graph_handle& graph, std::uint32_t node_id, math::vector2 position) -> void;

  auto save_shader_graph(shader_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Wraps a serialized subtree as a new, unsaved prefab. Editor code uses scenes::scene_serializer::create_prefab_from_node instead.
   *
   * @param snapshot The serialized subtree.
   * @param name The prefab's name.
   *
   * @return The new prefab.
   */
  auto create_prefab(YAML::Node snapshot, std::string name) -> prefab_handle;

  auto load_prefab(const math::uuid& id) -> prefab_handle;

  auto load_prefab(const std::filesystem::path& path) -> prefab_handle;

  /**
   * @brief Overwrites a prefab's snapshot in place; every handle sees the change. Pair with save_prefab to persist.
   *
   * @param prefab The prefab to update.
   * @param snapshot The new snapshot.
   */
  auto update_prefab(prefab_handle& prefab, YAML::Node snapshot) -> void;

  /**
   * @brief Writes a prefab to a `.prefab` file and registers it as an asset.
   *
   * @param prefab The prefab to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The prefab's canonical uuid.
   */
  auto save_prefab(prefab_handle& prefab, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Wraps a scene snapshot built by scenes::scene_serializer as a new, unsaved scene asset.
   *
   * @param snapshot The scene snapshot.
   * @param name The scene's name.
   *
   * @return The new scene.
   */
  auto create_scene(YAML::Node snapshot, std::string name) -> scene_handle;

  auto load_scene(const math::uuid& id) -> scene_handle;

  auto load_scene(const std::filesystem::path& path) -> scene_handle;

  /**
   * @brief Overwrites a scene's snapshot in place; every handle sees the change. Pair with save_scene to persist.
   *
   * @param scene The scene to update.
   * @param snapshot The new snapshot.
   */
  auto update_scene(scene_handle& scene, YAML::Node snapshot) -> void;

  /**
   * @brief Writes a scene to a `.scene` file and registers it as an asset.
   *
   * @param scene The scene to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The scene's canonical uuid.
   */
  auto save_scene(scene_handle& scene, const std::filesystem::path& path) -> math::uuid;

  /** @brief Finalizes finished background loads (budgeted) and queues their GPU uploads. Main thread, while the render thread is idle. */
  auto drain_loader_results() -> void;

  /**
   * @brief Turns queued uploads into GPU resources and bindless writes. Render thread.
   *
   * @param frame_index The frame the copies are recorded for.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture2d_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  [[nodiscard]] auto is_resident(const font_handle& font) const -> bool;

  [[nodiscard]] auto is_resident(const texture2d_array_handle& array) const -> bool;

  /**
   * @brief Live counts across every asset cache.
   *
   * @return The counts.
   */
  [[nodiscard]] auto resident_asset_counts() const -> assets::resident_asset_counts {
    return _residency.resident_asset_counts();
  }

  /**
   * @brief The image view behind a resident texture, for ImGui previews.
   *
   * @param texture The texture.
   *
   * @return The view, or VK_NULL_HANDLE if not uploaded yet.
   */
  [[nodiscard]] auto image_view_of(const texture2d_handle& texture) const -> VkImageView {
    return _residency.image_view_of(texture);
  }

  [[nodiscard]] auto white_texture() const noexcept -> texture2d_handle {
    return _residency.white_texture();
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture2d_handle {
    return _residency.normal_texture();
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture2d_handle {
    return _residency.black_texture();
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture2d_handle {
    return _residency.magenta_texture();
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _residency.material_buffer_address();
  }

  /**
   * @brief The bindless index of the global BRDF LUT, baked on the first environment map load and shared by all.
   *
   * @return The index, or `environment_map::invalid_index` before any environment map loaded.
   */
  [[nodiscard]] auto brdf_lut_index() const noexcept -> std::uint32_t {
    return _ibl.brdf_lut_index();
  }

  /**
   * @brief The project-relative path an asset was imported from.
   *
   * @param id The asset's uuid.
   *
   * @return The path, or empty if unknown.
   */
  [[nodiscard]] auto path_of(const math::uuid& id) const -> std::filesystem::path;

private:

  /**
   * @brief Rewrites `.material` files whose texture slots point at `old_relative` to `new_relative`, after move_asset moved a texture. Materials are the only assets referencing others by path.
   *
   * @param old_relative The texture's previous assets-relative path.
   * @param new_relative The texture's new assets-relative path.
   */
  auto _fixup_material_texture_references(const std::filesystem::path& old_relative, const std::filesystem::path& new_relative) -> void;

  asset_manifest _manifest{};
  ibl_baker _ibl{};
  asset_residency _residency;

  // Prefabs and scenes have no GPU state, so they bypass asset_residency and are cached here.
  std::unordered_map<math::uuid, prefab_handle> _prefabs{};

  std::unordered_map<math::uuid, scene_handle> _scenes{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
