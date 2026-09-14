// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
#define LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/font.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/skeleton.hpp>
#include <libsbx/assets/animation_clip.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/shader_graph.hpp>
#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/asset_manifest.hpp>
#include <libsbx/assets/asset_loader.hpp>
#include <libsbx/assets/ibl_baker.hpp>

namespace sbx::assets {

/**
 * @brief Turns cooked asset data into GPU-resident textures/meshes/materials/environment-maps:
 * upload queues, bindless registration, the material UBO, and the default fallback textures.
 * Depends on @ref asset_manifest for path/uuid/staleness bookkeeping and @ref ibl_baker for
 * environment baking -- both held by reference, owned by whoever constructs this (see @ref
 * assets_module).
 *
 * Every `load_*(uuid)` (bar @ref load_environment_map, see its own doc comment) creates a
 * placeholder record synchronously here on the calling thread -- reserving a bindless index where
 * one exists, no decoded/parsed content yet -- caches it immediately, and hands the actual disk
 * I/O/decode off to @ref asset_loader's background thread. The handle it returns is always valid
 * immediately; it simply doesn't finish (is_resident()/is_valid()-after-finalize, depending on the
 * type) until process_uploads' next call has drained the corresponding result and applied it. This
 * class alone interprets that raw data -- resolving nested asset references (paths -> uuids ->
 * handles), reserving GPU-adjacent identity, queuing the actual GPU upload -- the background thread
 * never does.
 */
class asset_residency final : public utility::noncopyable {

public:

  asset_residency(asset_manifest& manifest, ibl_baker& baker);

  ~asset_residency();

  /** @brief Loads a texture from a UUID or project-relative path; returns the existing handle if already loaded. */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture_handle;

  /** @brief Loads a TTF -> SDF glyph atlas font from a UUID or project-relative path; returns the existing handle if already loaded. */
  auto load_font(const math::uuid& id) -> font_handle;

  auto load_font(const std::filesystem::path& path) -> font_handle;

  /**
   * @brief Loads a mesh from a UUID or project-relative path; returns the existing handle if
   * already loaded, unless @p force_recook -- used only by the editor's mesh import-settings
   * dialog, since primitive/animation-clip selection changes what's actually *in* the cooked blob
   * (unlike extract_materials, which only affects finalize-time behavior on an already-cooked,
   * always-complete blob and is fine to stay ephemeral). @p force_recook against an
   * already-resident mesh re-cooks and re-finalizes it in place -- the existing handle stays
   * valid, its content just changes once the reload completes.
   */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  /**
   * @brief Builds a mesh directly from in-memory vertex/index data instead of a glTF import --
   * for procedurally generated geometry (terrain chunks, a runtime-authored road network mesh).
   * Uploaded the same way as a cooked mesh, via the next process_uploads() call. Not registered
   * in the uuid-keyed load_mesh cache -- each call always creates a new mesh record.
   */
  auto create_mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, std::vector<mesh::submesh> submeshes, const math::volume& bounds) -> mesh_handle;

  /** @brief Loads a skeleton cooked as a side effect of a mesh import; returns the existing handle if already loaded. Pure CPU data -- no GPU upload wait, but still resolved off the background thread like everything else. */
  auto load_skeleton(const math::uuid& id) -> skeleton_handle;

  /** @brief Loads an animation clip cooked as a side effect of a mesh import; returns the existing handle if already loaded. Pure CPU data -- no GPU upload wait, but still resolved off the background thread like everything else. */
  auto load_animation_clip(const math::uuid& id) -> animation_clip_handle;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites an existing material's fields in place. Every material_handle already
   * pointing at this record observes the change immediately, since they all share the same
   * underlying object. Does not touch identity (index/uuid) or persist to disk.
   */
  auto update_material(material_handle& material, const material::create_info& create_info) -> void;

  /**
   * @brief Writes a material to a `.material` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The material's canonical uuid (also written back onto the material record itself).
   */
  auto save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Loads and bakes an environment map -- the one asset kind that stays entirely
   * synchronous/main-thread, never touching the background loader. ibl_baker::bake_environment
   * does a real, blocking GPU compute dispatch (radiance upload + irradiance/prefiltered cubemap
   * bake); submitting that from a non-render thread is a fundamentally different, riskier problem
   * than disk I/O offload and is out of scope here.
   */
  auto load_environment_map(const math::uuid& id) -> environment_map_handle;

  auto load_environment_map(const std::filesystem::path& path) -> environment_map_handle;

  auto load_particle_effect(const math::uuid& id) -> particle_effect_handle;

  auto load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle;

  auto create_particle_effect(const particle_effect::create_info& create_info) -> particle_effect_handle;

  /**
   * @brief Overwrites an existing particle_effect's emitters in place. Every particle_effect_handle
   * already pointing at this record observes the change immediately. Does not touch identity
   * (uuid) or persist to disk.
   */
  auto update_particle_effect(particle_effect_handle& effect, const particle_effect::create_info& create_info) -> void;

  /**
   * @brief Writes a particle_effect to a `.particle_effect` file and (re-)registers it as a first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The effect's canonical uuid (also written back onto the record itself).
   */
  auto save_particle_effect(particle_effect_handle& effect, const std::filesystem::path& path) -> math::uuid;

  auto load_animation_graph(const math::uuid& id) -> animation_graph_handle;

  auto load_animation_graph(const std::filesystem::path& path) -> animation_graph_handle;

  auto create_animation_graph(const animation_graph::create_info& create_info) -> animation_graph_handle;

  /**
   * @brief Overwrites an existing animation_graph's states/transitions/parameters in place. Every
   * animation_graph_handle already pointing at this record observes the change immediately. Does
   * not touch identity (uuid) or persist to disk.
   */
  auto update_animation_graph(animation_graph_handle& graph, const animation_graph::create_info& create_info) -> void;

  /**
   * @brief Writes an animation_graph to a `.animation_graph` file and (re-)registers it as a
   * first-class asset.
   * @param path Destination path relative to the active project's assets directory.
   * @return The graph's canonical uuid (also written back onto the record itself).
   */
  auto save_animation_graph(animation_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  auto load_shader_graph(const math::uuid& id) -> shader_graph_handle;

  auto load_shader_graph(const std::filesystem::path& path) -> shader_graph_handle;

  auto create_shader_graph(const shader_graph::create_info& create_info) -> shader_graph_handle;

  /**
   * @brief Overwrites an existing shader_graph's nodes/edges in place. Every shader_graph_handle
   * already pointing at this record observes the change immediately. Does not touch identity
   * (uuid) or persist to disk.
   */
  auto update_shader_graph(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  /**
   * @brief Writes a shader_graph to a `.shadergraph` file, (re-)registers it as a first-class
   * asset, and cooks it (runs codegen, writes the generated `.slang` -- see
   * asset_cooker::cook_shader_graph's doc comment for why that lands inside the engine's shaders
   * tree rather than the usual per-project cooked-cache directory).
   * @param path Destination path relative to the active project's assets directory.
   * @return The graph's canonical uuid (also written back onto the record itself).
   */
  auto save_shader_graph(shader_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Drains the background loader's per-type result queues (budgeted) and turns queued
   * texture loads into GPU images and bindless writes (also budgeted).
   *
   * Runs on the render thread; copies are recorded by the caller's subsequent @ref upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  [[nodiscard]] auto is_resident(const font_handle& font) const -> bool;

  /**
   * @brief The image view backing a resident texture's bindless slot, or VK_NULL_HANDLE if the
   * texture isn't valid or its upload hasn't been processed yet (see is_resident). Used by the
   * editor/UI layer to blit a real preview into ImGui (see ui_module::texture_id), the same way
   * the viewport blits the scene's final image.
   */
  [[nodiscard]] auto image_view_of(const texture_handle& texture) const -> VkImageView;

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

  // Combined, shared across every result/pending-upload category drained per process_uploads()
  // call -- see the doc comments on _drain_loader_results and the pending-upload loop below.
  // Spreads a burst of loads/uploads across several frames instead of spiking one.
  inline static constexpr auto max_uploads_per_frame = std::size_t{32u};

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
    std::vector<skin_vertex> skin_vertices{}; // empty when the mesh has no skin data
  }; // struct pending_mesh_upload

  struct pending_material_upload {
    std::shared_ptr<material> record;
  }; // struct pending_material_upload

  auto _create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle;

  auto _register_material(std::shared_ptr<material> record) -> material_handle;

  /**
   * @brief Turns one already-cooked embedded glTF material (see cooked_submesh::material's doc
   * comment in asset_cooker.hpp) into a real, standalone `.material` asset next to the mesh
   * (models/<name>/materials/<material name>.material), reusing one already there instead of
   * overwriting it -- what mesh_import_options::extract_materials means. Runs on the main thread,
   * during _finalize_mesh, once per submesh that needs it.
   */
  auto _extract_gltf_material(const math::uuid& cooked_material_id, const std::filesystem::path& mesh_source) -> material_handle;

  [[nodiscard]] auto _texture_cache_key(const math::uuid& id, graphics::format format) const -> std::string;

  /**
   * @brief Pops up to max_uploads_per_frame entries combined across every asset_loader result
   * queue (roughly cost/frequency order: textures, meshes, fonts, materials, particle_effects,
   * animation_graphs, shader_graphs, skeletons, animation_clips) and runs each one's _finalize_* -- the one place
   * nested asset references (paths -> uuids -> handles) get resolved and GPU uploads get queued.
   * Called first thing inside process_uploads.
   */
  auto _drain_loader_results() -> void;

  auto _finalize_texture(asset_loader::texture_result& result) -> void;
  auto _finalize_mesh(asset_loader::mesh_result& result) -> void;
  auto _finalize_font(asset_loader::font_result& result) -> void;
  auto _finalize_material(asset_loader::material_result& result) -> void;
  auto _finalize_particle_effect(asset_loader::particle_effect_result& result) -> void;
  auto _finalize_animation_graph(asset_loader::animation_graph_result& result) -> void;
  auto _finalize_shader_graph(asset_loader::shader_graph_result& result) -> void;
  auto _finalize_skeleton(asset_loader::skeleton_result& result) -> void;
  auto _finalize_animation_clip(asset_loader::animation_clip_result& result) -> void;

  asset_manifest& _manifest;
  ibl_baker& _ibl;

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, std::shared_ptr<texture>> _textures{};
  std::deque<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  std::unordered_map<math::uuid, std::shared_ptr<font>> _fonts{};

  std::unordered_map<math::uuid, std::shared_ptr<mesh>> _meshes{};
  std::deque<pending_mesh_upload> _pending_meshes{};

  // Pure CPU data -- no GPU buffer/index, unlike _meshes above.
  std::unordered_map<math::uuid, std::shared_ptr<skeleton>> _skeletons{};
  std::unordered_map<math::uuid, std::shared_ptr<animation_clip>> _animation_clips{};

  std::vector<std::shared_ptr<material>> _materials{};
  std::deque<pending_material_upload> _pending_materials{};
  graphics::buffer_handle _material_buffer{};
  graphics::buffer::address_type _material_address{0u};
  std::uint32_t _material_count{0u};
  std::unordered_map<math::uuid, std::shared_ptr<material>> _material_files{};

  std::unordered_map<math::uuid, std::shared_ptr<environment_map>> _environment_maps{};

  // Pure CPU data — no GPU buffer/index, unlike _materials above.
  std::unordered_map<math::uuid, std::shared_ptr<particle_effect>> _particle_effect_files{};
  std::unordered_map<math::uuid, std::shared_ptr<animation_graph>> _animation_graph_files{};
  std::unordered_map<math::uuid, std::shared_ptr<shader_graph>> _shader_graph_files{};

  texture_handle _white{};
  texture_handle _normal{};
  texture_handle _black{};
  texture_handle _magenta{};

  // Declared LAST -- destroyed (aborted + joined) before any cache/pending-upload queue above that
  // its background thread might still be about to feed.
  asset_loader _loader{};

}; // class asset_residency

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
