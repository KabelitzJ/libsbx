// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_COOKER_HPP_
#define LIBSBX_ASSETS_ASSET_COOKER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/font.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/shader_graph.hpp>

namespace sbx::assets {

inline constexpr auto texture_cook_version = std::uint32_t{1u};
inline constexpr auto font_cook_version = std::uint32_t{1u};
inline constexpr auto environment_cook_version = std::uint32_t{1u};
inline constexpr auto material_cook_version = std::uint32_t{6u};
inline constexpr auto skeleton_cook_version = std::uint32_t{1u};
inline constexpr auto animation_cook_version = std::uint32_t{1u};
inline constexpr auto mesh_cook_version = std::uint32_t{10u};

// A mesh cook also emits its materials, skeleton and animation clips, so its freshness depends on all four cookers.
inline constexpr auto mesh_cooker_version = mesh_cook_version * 1000000u + material_cook_version * 10000u + skeleton_cook_version * 100u + animation_cook_version;

/**
 * @brief Editor import choices for a mesh.
 *
 * `extract_materials` only affects asset_residency's finalize step; the other fields change the cooked blob itself, so changing them requires a forced re-cook.
 */
struct mesh_import_options {
  /** @brief Extracts embedded materials into editable `.material` assets, reusing existing ones. */
  bool extract_materials{true};

  /** @brief Cooks the skin data, skeleton and animation clips; off loads the mesh as unskinned. */
  bool import_skeleton{true};

  /** @brief Which primitives, in mesh_source_summary::primitives order, become submeshes. Empty means all. */
  std::vector<std::size_t> included_primitives{};

  /** @brief Which animation clips to cook, by original index in the source's animation list. Empty means all. */
  std::vector<std::size_t> included_animations{};
}; // struct mesh_import_options

/** @brief One selectable primitive found by asset_cooker::inspect_mesh_source. */
struct mesh_source_primitive_summary {
  std::string mesh_name;        // the glTF mesh's name, or "Mesh {index}"
  std::size_t primitive_index;  // position within that glTF mesh
}; // struct mesh_source_primitive_summary

/** @brief A `.gltf`/`.glb` file's structure without cooking, for the mesh import dialog. Indices match mesh_import_options' index spaces. */
struct mesh_source_summary {
  std::vector<mesh_source_primitive_summary> primitives{};
  bool has_skeleton{false};
  std::vector<std::string> animation_names{};
}; // struct mesh_source_summary

/** @brief Decoded, GPU-independent pixel data. */
struct pixel_data {
  std::vector<std::byte> pixels;
  std::uint32_t width{0u};
  std::uint32_t height{0u};
}; // struct pixel_data

/** @brief A cooked SDF font atlas and glyph table. */
struct cooked_font_data {
  pixel_data atlas;
  std::vector<font::glyph> glyphs;
  std::uint32_t first_codepoint{0u};
  std::float_t line_height{0.0f};
  std::float_t ascent{0.0f};
  std::float_t descent{0.0f};
}; // struct cooked_font_data

/** @brief One coarser LOD level: an index range into the same vertex buffer as LOD0. */
struct mesh_lod {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::float_t error; // meshopt_simplify's relative error
}; // struct mesh_lod

struct cooked_submesh {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  math::volume bounds;

  // A self-cooked uuid for the embedded material (nil if none); asset_residency decides at finalize whether to extract it into a `.material`.
  math::uuid material;

  std::vector<mesh_lod> lods{}; // coarser levels beyond LOD0; may be empty
}; // struct cooked_submesh

struct cooked_mesh_data {
  std::vector<vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<cooked_submesh> submeshes;
  math::volume bounds;

  // Empty/nil without a glTF skin. skin_vertices is parallel to vertices.
  std::vector<skin_vertex> skin_vertices{};
  math::uuid skeleton{math::uuid::nil()};
  std::vector<math::uuid> animation_clips{};
}; // struct cooked_mesh_data

/** @brief Cooked animation clip data. */
struct animation_clip_data {
  std::string name;
  std::float_t duration{0.0f};
  std::vector<animation_joint_channel> channels;
}; // struct animation_clip_data

/** @brief A material's fields with texture paths instead of resolved handles, whether from glTF, a cooked blob or a `.material` file. */
struct material_description {
  std::string name{"material"};
  math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
  math::vector3 emissive_factor{0.0f, 0.0f, 0.0f};
  std::float_t metallic_factor{1.0f};
  std::float_t roughness_factor{1.0f};
  alpha_mode alpha{alpha_mode::opaque};
  shading_model shading{shading_model::pbr};
  std::float_t alpha_cutoff{0.5f};
  bool is_double_sided{false};
  bool casts_shadow{true};
  bool receives_shadow{true};
  std::float_t normal_scale{1.0f};
  std::float_t occlusion_strength{1.0f};
  std::float_t emissive_strength{1.0f};
  std::float_t ior{1.5f};
  math::vector2 uv_tiling{1.0f, 1.0f};
  math::vector2 uv_offset{0.0f, 0.0f};

  // Assets-relative paths (empty = no slot), not uuids: these are parsed off the main thread, and asset_residency resolves them when finalizing.
  std::string albedo{};
  std::string normal{};
  std::string metallic_roughness{};
  std::string occlusion{};
  std::string emissive{};

  // Path to a `.shadergraph` (empty = built-in shading). Not part of the cooked binary format.
  std::string shader_graph{};
  std::array<math::vector4, shader_graph_max_params> generic_params{};
  std::array<std::string, shader_graph_max_textures> generic_texture_paths{};

  // Per generic texture slot: load as linear instead of sRGB, for data like normal maps or masks.
  std::array<bool, shader_graph_max_textures> generic_texture_linear{};

  // Path to a hand-written `.slang` for shading_model::shader_code. Not part of the cooked binary format.
  std::string shader_code{};
}; // struct material_description

/** @brief A particle emitter's fields with assets-relative paths instead of handles, resolved by asset_residency when finalizing. */
struct particle_emitter_description {
  std::string name{"emitter"};
  particle_simulation_mode simulation_mode{particle_simulation_mode::cpu};
  emitter_blend_mode blend_mode{emitter_blend_mode::additive};
  std::float_t emission_rate{10.0f};
  std::uint32_t burst_count{0u};
  emitter_shape shape{emitter_shape::point};
  math::vector3 shape_extents{0.0f, 0.0f, 0.0f};
  cone_shape_params cone{};
  math::vector3 velocity_min{-1.0f, 1.0f, -1.0f};
  math::vector3 velocity_max{1.0f, 2.0f, 1.0f};
  std::float_t lifetime_min{1.0f};
  std::float_t lifetime_max{2.0f};
  math::color start_color{1.0f, 1.0f, 1.0f, 1.0f};
  math::color end_color{1.0f, 1.0f, 1.0f, 0.0f};
  gradient color_over_lifetime{};
  std::float_t size_min{0.1f};
  std::float_t size_max{0.2f};
  curve size_over_lifetime{};
  std::float_t rotation_min{0.0f};
  std::float_t rotation_max{0.0f};
  curve rotation_over_lifetime{};
  vector3_curve velocity_over_lifetime{};
  math::vector3 force_over_lifetime_min{0.0f, 0.0f, 0.0f};
  math::vector3 force_over_lifetime_max{0.0f, 0.0f, 0.0f};
  std::float_t gravity{0.0f};
  std::float_t drag{0.0f};
  std::string texture{};
  particle_render_mode render_mode{particle_render_mode::billboard};
  std::string render_mesh{};
  std::string render_material{};
  collision_config collision{};

  struct sub_emitter_description {
    sub_emitter_event event{sub_emitter_event::birth};
    std::string effect{};
    std::float_t probability{1.0f};
    bool inherit_velocity{false};
  }; // struct sub_emitter_description

  std::vector<sub_emitter_description> sub_emitters{};
  trail_config trail{};
}; // struct particle_emitter_description

struct particle_effect_description {
  std::string name{"particle_effect"};
  std::vector<particle_emitter_description> emitters{};
}; // struct particle_effect_description

/** @brief shader_graph_node_value with a texture path instead of a handle, resolved by asset_residency when finalizing. */
using shader_graph_node_value_description = std::variant<std::monostate, std::float_t, math::vector2, math::vector3, math::vector4, math::color, std::string>;

struct shader_graph_node_description {
  std::uint32_t id{0u};
  shader_node_type type{shader_node_type::constant_float};
  math::vector2 editor_position{0.0f, 0.0f};
  std::string name{};
  bool exposed{false};
  bool preview{false};
  shader_graph_node_value_description value{};
}; // struct shader_graph_node_description

struct shader_graph_description {
  std::string name{"shader_graph"};
  std::vector<shader_graph_node_description> nodes{};
  std::vector<shader_graph_edge> edges{};
}; // struct shader_graph_description

/**
 * @brief Cooks source assets (glTF, images, HDR, TTF) and parses YAML assets into decoded data or versioned on-disk caches.
 *
 * Stateless and static, so any thread can call it. Path, uuid and staleness bookkeeping live in asset_manifest; callers pass resolved paths and a needs_cook flag.
 */
class asset_cooker final {

public:

  /**
   * @brief Where an asset's cooked blob lives, whether or not it exists yet.
   *
   * @param id The asset's uuid.
   * @param extension The cooked file extension.
   *
   * @return The cooked path.
   */
  [[nodiscard]] static auto cooked_path(const math::uuid& id, std::string_view extension) -> std::filesystem::path;

  /** @brief Writes a cooked mesh blob from in-memory geometry, for built-in meshes without a source file. Loadable with resolve_mesh and needs_cook false. */
  [[nodiscard]] static auto write_cooked_mesh(const std::filesystem::path& cooked, const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const std::vector<cooked_submesh>& submeshes, const math::volume& bounds) -> bool;

  /** @brief Writes a cooked material blob keyed by @p id from a description, for built-in materials without a source file. */
  [[nodiscard]] static auto write_cooked_material(const math::uuid& id, const material_description& description) -> bool;

  /**
   * @brief Reads a material cooked from a mesh import.
   *
   * @param id The derived material uuid.
   *
   * @return The material, or nullopt if unreadable.
   */
  [[nodiscard]] static auto resolve_cooked_material(const math::uuid& id) -> std::optional<material_description>;

  /**
   * @brief The uuid of a mesh's Nth embedded material, derived from (mesh, index).
   *
   * @param mesh The mesh's uuid.
   * @param index The material's index in the source.
   *
   * @return The derived uuid.
   */
  [[nodiscard]] static auto derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  /**
   * @brief The uuid of a skinned mesh's skeleton, derived from the mesh's uuid.
   *
   * @param mesh The mesh's uuid.
   *
   * @return The derived uuid.
   */
  [[nodiscard]] static auto derive_skeleton_uuid(const math::uuid& mesh) -> math::uuid;

  /**
   * @brief The uuid of a mesh's animation clip, derived from (mesh, original clip index).
   *
   * @param mesh The mesh's uuid.
   * @param index The clip's original index in the source.
   *
   * @return The derived uuid.
   */
  [[nodiscard]] static auto derive_animation_clip_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  /**
   * @brief Cooks a texture if `needs_cook` or the cached blob is unreadable, then reads it.
   *
   * @param did_cook Set when a cook happened; the caller should then asset_manifest::record_cook.
   */
  [[nodiscard]] static auto resolve_texture(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data>;

  /** @brief Same as resolve_texture, for an equirectangular HDR environment map. */
  [[nodiscard]] static auto resolve_environment(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data>;

  /** @brief Same as resolve_texture, for a TTF glyph atlas. */
  [[nodiscard]] static auto resolve_font(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<cooked_font_data>;

  /**
   * @brief Same as resolve_texture, for a glTF mesh; embedded materials, skeleton and clips are cooked alongside under uuids derived from `id`.
   *
   * @p options changes the cooked blob, so callers changing it must force `needs_cook`.
   */
  [[nodiscard]] static auto resolve_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options, bool needs_cook, bool& did_cook) -> std::optional<cooked_mesh_data>;

  /**
   * @brief Reads a glTF file's primitives, skeleton presence and clip names without decoding or cooking, for the mesh import dialog.
   *
   * @param source The `.gltf`/`.glb` file.
   *
   * @return The summary, or nullopt if it can't be parsed.
   */
  [[nodiscard]] static auto inspect_mesh_source(const std::filesystem::path& source) -> std::optional<mesh_source_summary>;

  /**
   * @brief The external `.bin` and image files a `.gltf` references, relative to its directory, so an import can copy them along.
   *
   * @param source The `.gltf` file.
   *
   * @return The references; empty for a `.glb` or an unparsable file.
   */
  [[nodiscard]] static auto gltf_external_file_references(const std::filesystem::path& source) -> std::vector<std::filesystem::path>;

  /**
   * @brief Reads a skeleton cooked from a mesh import.
   *
   * @param id The derived skeleton uuid.
   *
   * @return The joints, or nullopt if unreadable.
   */
  [[nodiscard]] static auto resolve_skeleton(const math::uuid& id) -> std::optional<std::vector<skeleton::joint>>;

  /**
   * @brief Reads an animation clip cooked from a mesh import.
   *
   * @param id The derived clip uuid.
   *
   * @return The clip, or nullopt if unreadable.
   */
  [[nodiscard]] static auto resolve_animation_clip(const math::uuid& id) -> std::optional<animation_clip_data>;

  /**
   * @brief Parses a `.material` file, keeping texture references as paths.
   *
   * @param source The file to parse.
   *
   * @return The description, or nullopt on a parse error.
   */
  [[nodiscard]] static auto parse_material_file(const std::filesystem::path& source) -> std::optional<material_description>;

  /**
   * @brief Parses a `.particle_effect` file, keeping asset references as paths.
   *
   * @param source The file to parse.
   *
   * @return The description, or nullopt on a parse error.
   */
  [[nodiscard]] static auto parse_particle_effect_file(const std::filesystem::path& source) -> std::optional<particle_effect_description>;

  /**
   * @brief Parses an `.animation_graph` file; it has no asset references.
   *
   * @param source The file to parse.
   *
   * @return The graph, or nullopt on a parse error.
   */
  [[nodiscard]] static auto parse_animation_graph_file(const std::filesystem::path& source) -> std::optional<animation_graph::create_info>;

  /**
   * @brief Parses a `.shadergraph` file, keeping texture references as paths.
   *
   * @param source The file to parse.
   *
   * @return The description, or nullopt on a parse error.
   */
  [[nodiscard]] static auto parse_shader_graph_file(const std::filesystem::path& source) -> std::optional<shader_graph_description>;

  /**
   * @brief Generates Slang for a shader graph and writes it to `<shaders root>/generated/<id>_<generation>.slang`, deleting older generations of the same graph.
   *
   * It lives in the shaders tree so its `#include`s resolve. The shader and pipeline caches never invalidate, so the generation in the path is what makes a re-cook take effect.
   */
  [[nodiscard]] static auto cook_shader_graph(const math::uuid& id, std::uint64_t generation, const shader_graph::create_info& create_info) -> bool;

  /**
   * @brief Computes Lengyel tangents for vertices[vertex_start, vertex_start + vertex_count); normals and UVs must already be set. Public so procedural meshes can use it too.
   *
   * @param vertices The vertices to update.
   * @param indices The primitive's triangle indices, offset by vertex_start.
   */
  static auto generate_tangents(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void;

private:

  static auto _cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  static auto _cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  static auto _cook_font(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_font(const std::filesystem::path& cooked, cooked_font_data& data) -> bool;

  /** @brief Computes area-weighted vertex normals for vertices[vertex_start, vertex_start + vertex_count); positions must already be set. */
  static auto _generate_normals(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void;

  /** @brief Optimizes a submesh's slice for GPU caches and appends a coarser LOD chain to `indices`. @p skin_vertices, when non-null, is kept parallel to @p vertices. */
  static auto _optimize_and_generate_lods(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count, std::vector<skin_vertex>* skin_vertices = nullptr) -> std::vector<mesh_lod>;

  static auto _cook_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options) -> bool;

  /** @brief @p animation_clip_original_indices receives each cooked clip's original index in the source. */
  static auto _load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds, std::vector<skin_vertex>& skin_vertices, std::vector<std::uint32_t>& animation_clip_original_indices) -> bool;

  static auto _cook_material(const math::uuid& id, const material_description& description) -> bool;

  static auto _cook_skeleton(const math::uuid& id, const std::vector<skeleton::joint>& joints) -> bool;

  static auto _load_cooked_skeleton(const math::uuid& id, std::vector<skeleton::joint>& joints) -> bool;

  static auto _cook_animation_clip(const math::uuid& id, const animation_clip_data& data) -> bool;

  static auto _load_cooked_animation_clip(const math::uuid& id, animation_clip_data& data) -> bool;

}; // class asset_cooker

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_COOKER_HPP_
