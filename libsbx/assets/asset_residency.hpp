// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
#define LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture2d.hpp>
#include <libsbx/assets/texture2d_array.hpp>
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

/** @brief Live counts of every asset type asset_residency caches. */
struct resident_asset_counts {
  std::size_t textures{0u};
  std::size_t meshes{0u};
  std::size_t fonts{0u};
  std::size_t materials{0u};
  std::size_t environment_maps{0u};
  std::size_t particle_effects{0u};
  std::size_t animation_graphs{0u};
  std::size_t shader_graphs{0u};
  std::size_t skeletons{0u};
  std::size_t animation_clips{0u};
}; // struct resident_asset_counts

/**
 * @brief Turns cooked asset data into GPU-resident assets: upload queues, bindless registration, the material buffer and the default fallback textures.
 *
 * Every `load_*` except load_environment_map creates and caches a placeholder record synchronously and hands disk I/O and decoding to asset_loader's thread.
 * The returned handle is valid immediately; drain_loader_results finalizes it (resolving nested references) and process_uploads uploads it.
 */
class asset_residency final : public utility::noncopyable {

public:

  asset_residency(asset_manifest& manifest, ibl_baker& baker);

  ~asset_residency();

  /**
   * @brief Loads a texture by uuid or project-relative path, or returns the existing handle if already loaded in @p format.
   *
   * @param id The texture's uuid.
   * @param format The format to load it as.
   *
   * @return The texture handle, valid immediately.
   */
  auto load_texture(const math::uuid& id, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture2d_handle;

  auto load_texture(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb) -> texture2d_handle;

  /**
   * @brief Allocates an empty texture registered as both a bindless sampled and storage image, cleared and transitioned to `general` before returning.
   *
   * @param width The width in pixels.
   * @param height The height in pixels.
   * @param format The image format.
   *
   * @return The texture handle.
   */
  auto create_storage_image(std::uint32_t width, std::uint32_t height, graphics::format format) -> texture2d_handle;

  /**
   * @brief Finds a resident texture by uuid under any format, unlike load_texture which keys on (uuid, format).
   *
   * @p id may also be a record's own texture2d::handle(), matched first and exactly, since that's what scripts hold.
   *
   * @param id The asset uuid or record handle.
   *
   * @return The texture, or an empty handle if none is resident.
   */
  [[nodiscard]] auto find_texture(const math::uuid& id) const -> texture2d_handle;

  /**
   * @brief A 2D texture array where layer i is layers[i].
   *
   * Every layer must be valid, resident, exactly @p size and share the first layer's format and mip count; otherwise this logs the offending layer and creates nothing.
   * Layers are copied on the GPU at the next upload flush, so keep them alive until the array is resident. Free it with release_texture2d_array.
   *
   * @param layers The source textures.
   * @param size The size every layer must have.
   *
   * @return The array, or an invalid handle on failure.
   */
  auto create_texture2d_array(std::span<const texture2d_handle> layers, const math::vector2u& size) -> texture2d_array_handle;

  [[nodiscard]] auto find_texture2d_array(const math::uuid& id) const -> texture2d_array_handle;

  /**
   * @brief Frees the array's image and bindless slot, not its layers.
   *
   * @param array The array to free.
   */
  auto release_texture2d_array(const texture2d_array_handle& array) -> void;

  /**
   * @brief Frees a texture's bindless indices, retires its image and drops it from the cache.
   *
   * Nothing frees created textures automatically. Never call this on a texture still referenced elsewhere or loaded through load_texture's cache.
   *
   * @param texture The texture to free.
   */
  auto release_texture(const texture2d_handle& texture) -> void;

  /**
   * @brief The GPU image behind a texture's sampled bindless index, e.g. for readback.
   *
   * @param texture The texture.
   *
   * @return The image, or an empty handle if the texture is invalid or not resident yet.
   */
  [[nodiscard]] auto image_handle_for(const texture2d_handle& texture) const -> graphics::image_handle;

  /**
   * @brief Loads a TTF as an SDF glyph atlas font by uuid or project-relative path, or returns the existing handle.
   *
   * @param id The font's uuid.
   *
   * @return The font handle, valid immediately.
   */
  auto load_font(const math::uuid& id) -> font_handle;

  auto load_font(const std::filesystem::path& path) -> font_handle;

  /**
   * @brief Loads a mesh by uuid or project-relative path, or returns the existing handle if already loaded.
   *
   * @p force_recook re-cooks an already-resident mesh in place (the handle stays valid); the editor's import settings use it because primitive and clip selection change the cooked blob.
   *
   * @param id The mesh's uuid.
   * @param options Import options used when cooking.
   * @param force_recook Whether to re-cook even if already loaded.
   *
   * @return The mesh handle, valid immediately.
   */
  auto load_mesh(const math::uuid& id, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  auto load_mesh(const std::filesystem::path& path, const mesh_import_options& options = {}, bool force_recook = false) -> mesh_handle;

  /**
   * @brief Builds a mesh from in-memory data, uploaded through the next process_uploads(). Every call creates a new, uncached record.
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
   * @brief Builds a mesh for frequently replaced content, writing host-visible buffers directly so it is resident when this returns.
   *
   * Call only while the render thread is idle. Every call creates new buffers; release_mesh the mesh being replaced or its buffers leak.
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
   * @brief Retires a mesh's GPU buffers (vertex, index and skin).
   *
   * A mesh_handle going out of scope never frees them, so call this before dropping a mesh from create_mesh or create_dynamic_mesh. No-op if the mesh is invalid or was never uploaded.
   *
   * @param mesh The mesh to release.
   */
  auto release_mesh(const mesh_handle& mesh) -> void;

  /**
   * @brief Loads a skeleton cooked from a mesh import, or returns the existing handle. CPU data only.
   *
   * @param id The skeleton's uuid.
   *
   * @return The skeleton handle, valid immediately.
   */
  auto load_skeleton(const math::uuid& id) -> skeleton_handle;

  /**
   * @brief Loads an animation clip cooked from a mesh import, or returns the existing handle. CPU data only.
   *
   * @param id The clip's uuid.
   *
   * @return The clip handle, valid immediately.
   */
  auto load_animation_clip(const math::uuid& id) -> animation_clip_handle;

  auto load_material(const math::uuid& id) -> material_handle;

  auto load_material(const std::filesystem::path& path) -> material_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Overwrites a material's fields in place; every handle to it sees the change. Doesn't change its identity or save it.
   *
   * @param material The material to update.
   * @param create_info The new fields.
   */
  auto update_material(material_handle& material, const material::create_info& create_info) -> void;

  /**
   * @brief Copies @p source into a new, registered material with its own uuid, for per-instance materials that must not affect the source.
   *
   * @param source The material to copy.
   *
   * @return The new material.
   */
  auto duplicate_material(const material_handle& source) -> material_handle;

  /**
   * @brief Frees a material's slot in the fixed-size material buffer and drops it from the cache.
   *
   * Handles going out of scope never free slots, so release instances you're done with or they consume material_capacity for good.
   * There is no reference count: never release a material still in use, such as a duplicate_material template.
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

  /**
   * @brief Loads and bakes an environment map synchronously on the main thread; the IBL bake is a blocking GPU dispatch.
   *
   * @param id The environment map's uuid.
   *
   * @return The environment map handle, fully baked.
   */
  auto load_environment_map(const math::uuid& id) -> environment_map_handle;

  auto load_environment_map(const std::filesystem::path& path) -> environment_map_handle;

  auto load_particle_effect(const math::uuid& id) -> particle_effect_handle;

  auto load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle;

  auto create_particle_effect(const particle_effect::create_info& create_info) -> particle_effect_handle;

  /**
   * @brief Overwrites a particle effect's emitters in place; every handle to it sees the change. Doesn't change its identity or save it.
   *
   * @param effect The effect to update.
   * @param create_info The new emitters.
   */
  auto update_particle_effect(particle_effect_handle& effect, const particle_effect::create_info& create_info) -> void;

  /**
   * @brief Writes a particle effect to a `.particle_effect` file and registers it as an asset.
   *
   * @param effect The effect to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The effect's canonical uuid.
   */
  auto save_particle_effect(particle_effect_handle& effect, const std::filesystem::path& path) -> math::uuid;

  auto load_animation_graph(const math::uuid& id) -> animation_graph_handle;

  auto load_animation_graph(const std::filesystem::path& path) -> animation_graph_handle;

  auto create_animation_graph(const animation_graph::create_info& create_info) -> animation_graph_handle;

  /**
   * @brief Overwrites an animation graph's states, transitions and parameters in place; every handle to it sees the change. Doesn't change its identity or save it.
   *
   * @param graph The graph to update.
   * @param create_info The new contents.
   */
  auto update_animation_graph(animation_graph_handle& graph, const animation_graph::create_info& create_info) -> void;

  /**
   * @brief Writes an animation graph to a `.animation_graph` file and registers it as an asset.
   *
   * @param graph The graph to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The graph's canonical uuid.
   */
  auto save_animation_graph(animation_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  auto load_shader_graph(const math::uuid& id) -> shader_graph_handle;

  auto load_shader_graph(const std::filesystem::path& path) -> shader_graph_handle;

  auto create_shader_graph(const shader_graph::create_info& create_info) -> shader_graph_handle;

  /**
   * @brief Overwrites a shader graph in place, bumps its generation and re-cooks it so the render passes pick up the change.
   *
   * Only used on save and on async-load finalize; live editor edits use update_shader_graph_data, since re-cooking on every edit stalls on a Slang compile.
   *
   * @param graph The graph to update.
   * @param create_info The new nodes and edges.
   */
  auto update_shader_graph(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  /**
   * @brief Overwrites a shader graph's nodes and edges in place without bumping its generation or re-cooking; cheap enough for every editor edit.
   *
   * @param graph The graph to update.
   * @param create_info The new nodes and edges.
   */
  auto update_shader_graph_data(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void;

  /**
   * @brief Moves one node without re-cooking, since editor positions never affect codegen. No-op if @p node_id doesn't exist.
   *
   * @param graph The graph containing the node.
   * @param node_id The node to move.
   * @param position The new editor position.
   */
  auto update_shader_graph_node_position(shader_graph_handle& graph, std::uint32_t node_id, math::vector2 position) -> void;

  /**
   * @brief Writes a shader graph to a `.shadergraph` file, registers it as an asset and re-cooks it; this is where editor changes take effect.
   *
   * @param graph The graph to save; receives the canonical uuid.
   * @param path Destination path relative to the active project's assets directory.
   *
   * @return The graph's canonical uuid.
   */
  auto save_shader_graph(shader_graph_handle& graph, const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Finalizes up to max_uploads_per_frame loader results: resolves nested asset references and queues GPU uploads.
   *
   * Main thread only, while the render thread is idle: finalizing writes records that packet building reads and fires on_loaded callbacks.
   */
  auto drain_loader_results() -> void;

  /**
   * @brief Turns queued texture, mesh and material uploads into GPU resources and bindless writes, within the per-frame budget. Render thread.
   *
   * @param frame_index The frame the copies are recorded for; the caller's upload_context::flush records them.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture2d_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto is_resident(const material_handle& material) const -> bool;

  [[nodiscard]] auto is_resident(const environment_map_handle& environment) const -> bool;

  [[nodiscard]] auto is_resident(const font_handle& font) const -> bool;

  [[nodiscard]] auto is_resident(const texture2d_array_handle& array) const -> bool;

  /**
   * @brief Live counts across every cache, for the editor's Statistics panel.
   *
   * @return The counts.
   */
  [[nodiscard]] auto resident_asset_counts() const -> assets::resident_asset_counts;

  /**
   * @brief The image view behind a resident texture's bindless slot, for ImGui previews.
   *
   * @param texture The texture.
   *
   * @return The image view, or VK_NULL_HANDLE if the texture is invalid or not uploaded yet.
   */
  [[nodiscard]] auto image_view_of(const texture2d_handle& texture) const -> VkImageView;

  [[nodiscard]] auto white_texture() const noexcept -> texture2d_handle {
    return _white;
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture2d_handle {
    return _normal;
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture2d_handle {
    return _black;
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture2d_handle {
    return _magenta;
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _material_address;
  }

private:

  inline static constexpr auto material_capacity = std::uint32_t{1024u};

  // Shared budget across every category drained per frame, spreading load bursts over several frames.
  inline static constexpr auto max_uploads_per_frame = std::size_t{32u};

  struct pending_texture_upload {
    std::uint32_t index;
    std::vector<std::byte> pixels;
    std::uint32_t width;
    std::uint32_t height;
    graphics::format format;
    // False for data mips would corrupt, like SDF font atlases where averaging drops thin strokes.
    bool mipmapped{true};
  }; // struct pending_texture_upload

  struct pending_array_upload {
    std::shared_ptr<texture2d_array> record;
    graphics::image_handle image;
    std::vector<graphics::image_handle> layers;
  }; // struct pending_array_upload

  struct pending_mesh_upload {
    std::shared_ptr<mesh> record;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<skin_vertex> skin_vertices{}; // empty without skin data
  }; // struct pending_mesh_upload

  // Must stay byte-identical to material_data in frame_data.slang.
  struct material_data {
    math::vector4 base_color_factor;
    math::vector4 emissive_factor;
    std::uint32_t albedo_index;
    std::uint32_t normal_index;
    std::uint32_t metallic_roughness_index;
    std::uint32_t occlusion_index;
    std::uint32_t emissive_index;
    std::float_t metallic_factor;
    std::float_t roughness_factor;
    std::float_t alpha_cutoff;
    std::uint32_t flags;
    std::float_t normal_scale;
    std::float_t occlusion_strength;
    std::float_t emissive_strength;
    std::float_t ior;
    math::vector2 uv_tiling;
    math::vector2 uv_offset;
    // One float4 per exposed param regardless of type, avoiding packing and alignment pitfalls.
    math::vector4 generic_params[shader_graph_max_params];
    std::uint32_t generic_textures[shader_graph_max_textures];
  }; // struct material_data

  // Snapshotted on the main thread so the render thread never reads a record being edited.
  struct pending_material_upload {
    std::uint32_t index;
    material_data data;
  }; // struct pending_material_upload

  auto _create_default_texture(std::array<std::uint8_t, 4u> color) -> texture2d_handle;

  auto _register_material(std::shared_ptr<material> record) -> material_handle;

  [[nodiscard]] auto _material_data_of(const material& material) const -> material_data;

  /**
   * @brief Extracts an embedded glTF material into a standalone `.material` next to the mesh, reusing an existing one. Main thread, during _finalize_mesh.
   *
   * @param cooked_material_id The cooked embedded material.
   * @param mesh_source The mesh's source path.
   *
   * @return The extracted material.
   */
  auto _extract_gltf_material(const math::uuid& cooked_material_id, const std::filesystem::path& mesh_source) -> material_handle;

  [[nodiscard]] auto _texture_cache_key(const math::uuid& id, graphics::format format) const -> std::string;

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

  std::unordered_map<std::string, std::shared_ptr<texture2d>> _textures{};
  std::deque<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  // Texture arrays by id; images and resident frames by index in the 2D-array binding, a separate index space from _images.
  std::unordered_map<math::uuid, std::shared_ptr<texture2d_array>> _texture_arrays{};
  std::deque<pending_array_upload> _pending_arrays{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _array_images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _array_resident_frame{};

  std::unordered_map<math::uuid, std::shared_ptr<font>> _fonts{};

  std::unordered_map<math::uuid, std::shared_ptr<mesh>> _meshes{};
  std::deque<pending_mesh_upload> _pending_meshes{};

  std::unordered_map<math::uuid, std::shared_ptr<skeleton>> _skeletons{};
  std::unordered_map<math::uuid, std::shared_ptr<animation_clip>> _animation_clips{};

  std::vector<std::shared_ptr<material>> _materials{};
  std::deque<pending_material_upload> _pending_materials{};
  graphics::buffer_handle _material_buffer{};
  graphics::buffer::address_type _material_address{0u};
  std::uint32_t _material_count{0u};
  std::vector<std::uint32_t> _free_material_indices{};
  std::unordered_map<math::uuid, std::shared_ptr<material>> _material_files{};

  std::unordered_map<math::uuid, std::shared_ptr<environment_map>> _environment_maps{};

  std::unordered_map<math::uuid, std::shared_ptr<particle_effect>> _particle_effect_files{};
  std::unordered_map<math::uuid, std::shared_ptr<animation_graph>> _animation_graph_files{};
  std::unordered_map<math::uuid, std::shared_ptr<shader_graph>> _shader_graph_files{};

  texture2d_handle _white{};
  texture2d_handle _normal{};
  texture2d_handle _black{};
  texture2d_handle _magenta{};

  // Declared last, so its thread is joined before the caches and queues it feeds are destroyed.
  asset_loader _loader{};

}; // class asset_residency

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_RESIDENCY_HPP_
