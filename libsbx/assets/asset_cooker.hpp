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
inline constexpr auto material_cook_version = std::uint32_t{6u}; // v6: adds shading_model (pbr/unlit) to the binary header
inline constexpr auto skeleton_cook_version = std::uint32_t{1u};
inline constexpr auto animation_cook_version = std::uint32_t{1u};
inline constexpr auto mesh_cook_version = std::uint32_t{9u}; // v9: mesh_import_options-driven primitive/animation-clip selection at cook time; cooked animation clips now keep their *original* source index (not a renumbered count) -- see _load_cooked_mesh's doc comment

// A mesh cook also emits its materials and, for a skinned mesh, its skeleton/animation clips -- so
// a mesh blob's freshness depends on all four cookers. Exposed (not file-local, unlike the cooked
// binary format's magic numbers) because is_cooked_stale/record_cook need it wherever a mesh load
// is prepared -- asset_residency's main-thread mesh finalize, and assets_module::resolve_mesh_
// collision_data's direct bypass of asset_residency.
inline constexpr auto mesh_cooker_version = mesh_cook_version * 1000000u + material_cook_version * 10000u + skeleton_cook_version * 100u + animation_cook_version;

/**
 * @brief Editor import-time choices for a mesh. `extract_materials` is consulted only by
 * asset_residency's mesh finalize step -- cooking itself always produces a self-contained,
 * resolvable material regardless of this flag (see cooked_submesh::material's doc comment).
 * `import_skeleton`/`included_primitives`/`included_animations` are consulted by the cook itself
 * (asset_cooker::resolve_mesh/_cook_mesh) -- they change what's actually *in* the cooked blob, so
 * (unlike extract_materials) a change to any of these requires a fresh cook of the same source to
 * take effect (see asset_residency::load_mesh's force_recook parameter).
 */
struct mesh_import_options {
  /** @brief Extracts a cooked mesh's embedded materials into standalone, editable `.material` assets (reusing an existing one rather than overwriting it). On by default. */
  bool extract_materials{true};

  /** @brief Cooks and resolves a skinned source mesh's skin data, skeleton, and animation clips at all. On by default. Off skips all three entirely -- the loaded mesh behaves as fully unskinned, with `included_animations` below moot. */
  bool import_skeleton{true};

  /** @brief Which primitives (in mesh_source_summary::primitives' order -- the Nth primitive _cook_mesh's own scene/mesh traversal encounters) become submeshes. Empty means every primitive. */
  std::vector<std::size_t> included_primitives{};

  /** @brief Which animation clips (by their *original* index into the source's animation list, not a renumbered "Nth selected" count -- see derive_animation_clip_uuid's doc comment) get cooked. Empty means every clip. Moot when import_skeleton is off. */
  std::vector<std::size_t> included_animations{};
}; // struct mesh_import_options

/** @brief One selectable primitive in a mesh_source_summary -- a preview of what asset_cooker::inspect_mesh_source found, before any cooking. */
struct mesh_source_primitive_summary {
  std::string mesh_name;        // the source glTF mesh's name, or "Mesh {index}" if it has none
  std::size_t primitive_index;  // this primitive's position within that glTF mesh (a glTF mesh can have more than one primitive)
}; // struct mesh_source_primitive_summary

/**
 * @brief What asset_cooker::inspect_mesh_source finds in a `.gltf`/`.glb` source file without
 * cooking anything -- the editor's mesh import-settings dialog's data source. `primitives`' and
 * `animation_names`' indices are exactly mesh_import_options::included_primitives'/
 * included_animations' index spaces (see those fields' doc comments for what each index means).
 */
struct mesh_source_summary {
  std::vector<mesh_source_primitive_summary> primitives{};
  bool has_skeleton{false};
  std::vector<std::string> animation_names{};
}; // struct mesh_source_summary

/** @brief Decoded, GPU-independent pixel data — the shared shape resolve_texture/resolve_environment hand back. */
struct pixel_data {
  std::vector<std::byte> pixels;
  std::uint32_t width{0u};
  std::uint32_t height{0u};
}; // struct pixel_data

/** @brief A cooked SDF font atlas + glyph table, ready for asset_residency::load_font. */
struct cooked_font_data {
  pixel_data atlas;
  std::vector<font::glyph> glyphs;
  std::uint32_t first_codepoint{0u};
  std::float_t line_height{0.0f};
  std::float_t ascent{0.0f};
  std::float_t descent{0.0f};
}; // struct cooked_font_data

/** @brief One coarser level in a submesh's LOD chain — an index range into the same shared vertex buffer as its LOD0. */
struct mesh_lod {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::float_t error; // meshopt_simplify's relative error metric for this level
}; // struct mesh_lod

struct cooked_submesh {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  math::volume bounds;

  // Always a real, resolvable uuid (or nil, for a primitive with no material at all) -- cooking
  // itself never asks asset_residency for anything, it always self-cooks each glTF material as a
  // side-effect blob (asset_cooker::derive_material_uuid + resolve_cooked_material), the same way
  // it already does for a skinned mesh's skeleton/animation clips. Whether to instead extract this
  // into a standalone, hand-editable `.material` file (mesh_import_options::extract_materials) is
  // asset_residency's main-thread mesh finalize step's decision, made *after* cooking, not
  // cooking's.
  math::uuid material;

  std::vector<mesh_lod> lods{}; // progressively coarser levels beyond index_offset/index_count (LOD0); may be empty
}; // struct cooked_submesh

struct cooked_mesh_data {
  std::vector<vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<cooked_submesh> submeshes;
  math::volume bounds;

  // Skinning -- empty/nil when the source mesh has no glTF skin. skin_vertices is parallel to
  // vertices (same count); skeleton/animation_clips are derived uuids ready to pass to
  // resolve_skeleton/resolve_animation_clip.
  std::vector<skin_vertex> skin_vertices{};
  math::uuid skeleton{math::uuid::nil()};
  std::vector<math::uuid> animation_clips{};
}; // struct cooked_mesh_data

/** @brief Raw (uuid-free -- a skeleton's joints hold no asset references) cooked animation data, keyed by resolve_animation_clip's uuid. */
struct animation_clip_data {
  std::string name;
  std::float_t duration{0.0f};
  std::vector<animation_joint_channel> channels;
}; // struct animation_clip_data

/** @brief A material's fields with texture *uuids*, not resolved handles — same shape whether it came from a glTF embed, a cooked blob, or a hand-authored `.material` YAML file. */
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

  // Assets-directory-relative paths (empty = no slot), *not* uuids -- resolving a path to a stable
  // uuid is asset_manifest::import's job, and asset_manifest is main-thread-only (see asset_
  // manifest.hpp's doc comment on why). A description produced off the background thread -- by
  // parse_material_file for a hand-authored `.material` YAML file, or by _cook_mesh for an
  // embedded glTF material -- can therefore never carry a real uuid for these; asset_residency's
  // material finalize step (main thread) turns each path into a handle via load_texture(path, ...),
  // which already does the assets_directory/import resolution internally, same as it always has.
  std::string albedo{};
  std::string normal{};
  std::string metallic_roughness{};
  std::string occlusion{};
  std::string emissive{};

  // Path to a `.shadergraph` asset (empty = built-in pbr/unlit via `shading`); see shader_graph.hpp
  // for the graph's own declared parameter list this material's generic_params/
  // generic_texture_paths supply values for, in slot order. Not carried by the cooked binary
  // format (.sbxmat) -- a glTF-embedded material has no meaningful way to reference one, unlike
  // every other field here.
  std::string shader_graph{};
  std::array<math::vector4, shader_graph_max_params> generic_params{};
  std::array<std::string, shader_graph_max_textures> generic_texture_paths{};
}; // struct material_description

/**
 * @brief A particle_effect's fields with asset *paths* (assets-directory-relative, empty = no
 * slot), not uuids or resolved handles -- see material_description's doc comment for why paths,
 * not uuids, are what a background-thread parse can produce. What asset_cooker::parse_particle_
 * effect_file hands back; asset_residency's particle_effect finalize step (main thread) resolves
 * each path into a handle via the matching load_*(path, ...) overload and calls the existing
 * update_particle_effect to build the live object.
 */
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

/**
 * @brief A shader_graph_node's fields with a texture *path* (empty = none) instead of a resolved
 * handle for its texture_sample payload -- see material_description's doc comment for why paths,
 * not handles, are what a background-thread parse can produce. Every other node type carries no
 * asset reference at all, so this only differs from shader_graph_node in that one variant
 * alternative.
 */
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
  std::vector<shader_graph_edge> edges{}; // no path-dependent fields -- reused as-is from shader_graph.hpp
}; // struct shader_graph_description

/** @brief Cooks source assets (glTF, images, HDR) and hand-authored YAML assets (`.material`, `.particle_effect`, `.animation_graph`) into decoded data or versioned on-disk caches. Holds no state and is never instantiated -- every member is static, self-contained given the inputs it's passed, so any thread can call e.g. asset_cooker::resolve_mesh(...) directly with no instance or coordination needed. Path/uuid/staleness bookkeeping lives in @ref asset_manifest instead -- resolve_ and parse_ calls take already-resolved source/cooked paths and a needs_cook flag rather than looking them up. */
class asset_cooker final {

public:

  /** @brief Where a given asset's cooked cache blob lives, regardless of whether it exists yet. */
  [[nodiscard]] static auto cooked_path(const math::uuid& id, std::string_view extension) -> std::filesystem::path;

  /**
   * @brief Writes a cooked mesh cache blob directly from in-memory geometry, bypassing glTF import
   * entirely -- for engine-generated meshes (built-in primitives) with no source file to cook from.
   * Same on-disk format _cook_mesh produces (unskinned, no animation clips), loadable via the
   * ordinary resolve_mesh call once written, passing needs_cook as false.
   */
  [[nodiscard]] static auto write_cooked_mesh(const std::filesystem::path& cooked, const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const std::vector<cooked_submesh>& submeshes, const math::volume& bounds) -> bool;

  /**
   * @brief Writes a cooked material cache blob directly from an in-memory description, keyed
   * purely by @p id -- same on-disk format _cook_material produces (a side-effect glTF material's
   * format), so it's loadable via the ordinary uuid-based resolve_cooked_material/load_material
   * call once written, no manifest entry needed (cooked_path(id, ...) is deterministic from id
   * alone). For engine-generated materials (the built-in primitives' default material) with no
   * source file to cook from.
   */
  [[nodiscard]] static auto write_cooked_material(const math::uuid& id, const material_description& description) -> bool;

  /** @brief Reads a material cooked as a side effect of a mesh import (not a hand-authored `.material` file). Stateless -- safe to call from any thread, no asset_cooker instance needed. */
  [[nodiscard]] static auto resolve_cooked_material(const math::uuid& id) -> std::optional<material_description>;

  /** @brief A mesh's Nth embedded glTF material's derived, self-cooked uuid -- deterministic from (mesh, index) alone, no lookup needed. */
  [[nodiscard]] static auto derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  /** @brief A skinned mesh's cooked skeleton's derived uuid -- deterministic from `mesh` alone. */
  [[nodiscard]] static auto derive_skeleton_uuid(const math::uuid& mesh) -> math::uuid;

  /** @brief A skinned mesh's Nth cooked animation clip's derived uuid -- deterministic from (mesh, index) alone. */
  [[nodiscard]] static auto derive_animation_clip_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  /**
   * @brief Cook (if `needs_cook`, or the cached blob turns out unreadable) + read a texture.
   * @param did_cook Set to true if a cook actually happened (whether because `needs_cook` was set,
   * or the cached blob was unreadable and had to be regenerated) -- the caller should
   * asset_manifest::record_cook when this comes back true.
   */
  [[nodiscard]] static auto resolve_texture(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data>;

  /** @brief Same shape as @ref resolve_texture, for an equirectangular HDR environment map. */
  [[nodiscard]] static auto resolve_environment(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<pixel_data>;

  /** @brief Same shape as @ref resolve_texture, for a TTF -> SDF glyph atlas. */
  [[nodiscard]] static auto resolve_font(const std::filesystem::path& source, const std::filesystem::path& cooked, bool needs_cook, bool& did_cook) -> std::optional<cooked_font_data>;

  /**
   * @brief Same shape as @ref resolve_texture, for a glTF mesh. Every embedded material, and (for
   * a skinned mesh) its skeleton/animation clips, are cooked as self-contained side-effect blobs --
   * see cooked_submesh::material's doc comment; `id` is needed to derive their uuids. @p options
   * selects which primitives/animation clips actually get cooked (see mesh_import_options' fields'
   * doc comments) -- unlike extract_materials, these change the cooked blob itself, so a caller
   * changing them must also force `needs_cook`, not rely on ordinary staleness detection.
   */
  [[nodiscard]] static auto resolve_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options, bool needs_cook, bool& did_cook) -> std::optional<cooked_mesh_data>;

  /**
   * @brief Reads a `.gltf`/`.glb` source file's top-level structure -- its primitives, whether it
   * has a skeleton, its animation clip names -- without decoding any vertex/keyframe data or
   * cooking anything. Stateless, safe from any thread, same as every other member here. The
   * editor's mesh import-settings dialog calls this directly to build its checkbox lists before
   * the user picks what to actually cook via @ref resolve_mesh.
   */
  [[nodiscard]] static auto inspect_mesh_source(const std::filesystem::path& source) -> std::optional<mesh_source_summary>;

  /**
   * @brief A loose (non-binary) `.gltf`'s external buffer (`.bin`) and image files, as paths
   * relative to @p source's own parent directory -- empty for a `.glb` (self-contained, nothing
   * external) or if @p source can't be parsed. Doesn't load any of their bytes (`fastgltf::Options::
   * None`, unlike @ref resolve_mesh's `LoadExternalBuffers`) -- just the reference list. The
   * editor's "Import from Disk..." needs this to copy those sibling files alongside the `.gltf`
   * itself; without them, both @ref inspect_mesh_source and @ref resolve_mesh fail outright, since
   * fastgltf can't resolve a buffer/image URI that was never copied to sit next to the imported
   * file's new location.
   */
  [[nodiscard]] static auto gltf_external_file_references(const std::filesystem::path& source) -> std::vector<std::filesystem::path>;

  /** @brief Reads a skeleton cooked as a side effect of a mesh import. @p id comes from @ref cooked_mesh_data::skeleton / @ref derive_skeleton_uuid. Pure read, no staleness tracking of its own (it's only ever produced alongside its owning mesh). */
  [[nodiscard]] static auto resolve_skeleton(const math::uuid& id) -> std::optional<std::vector<skeleton::joint>>;

  /** @brief Reads an animation clip cooked as a side effect of a mesh import. @p id comes from @ref cooked_mesh_data::animation_clips / @ref derive_animation_clip_uuid. */
  [[nodiscard]] static auto resolve_animation_clip(const math::uuid& id) -> std::optional<animation_clip_data>;

  /** @brief Parses a hand-authored `.material` YAML file into asset-path-referencing (not uuid- or handle-referencing) form -- see material_description's doc comment. */
  [[nodiscard]] static auto parse_material_file(const std::filesystem::path& source) -> std::optional<material_description>;

  /** @brief Same shape as @ref parse_material_file, for a `.particle_effect` YAML file. */
  [[nodiscard]] static auto parse_particle_effect_file(const std::filesystem::path& source) -> std::optional<particle_effect_description>;

  /** @brief Same shape as @ref parse_material_file, for an `.animation_graph` YAML file -- has no asset references at all today, so its create_info can be used as-is, no path/uuid indirection needed. */
  [[nodiscard]] static auto parse_animation_graph_file(const std::filesystem::path& source) -> std::optional<animation_graph::create_info>;

  /** @brief Same shape as @ref parse_material_file, for a `.shadergraph` YAML file -- see shader_graph_description's doc comment for why texture_sample nodes need the path indirection. */
  [[nodiscard]] static auto parse_shader_graph_file(const std::filesystem::path& source) -> std::optional<shader_graph_description>;

  /**
   * @brief Translates @p create_info to Slang (shader_graph_codegen.hpp) and writes it to
   * `<engine shaders root>/generated/<id>_<generation>.slang` (shader_graph_generated_path), *not*
   * the usual `cooked_path`/library-directory convention every other cooker uses -- `shader_compiler`
   * resolves a compiled file's `#include`s relative to the nearest "shaders" ancestor directory (see
   * shader_compiler.cpp's `_shaders_root`), so the generated file has to actually live inside that
   * tree (alongside `geometry_common.slang` etc.) for its `#include <geometry_common.slang>` to
   * resolve at all. No staleness tracking on the codegen itself -- it's cheap pure string generation
   * (no external tool), so this always regenerates -- but @p generation IS what makes a re-cook after
   * a live edit actually take effect: shader_cache/pipeline_cache are plain path-keyed maps with no
   * invalidation of their own (see shader_graph_generated_name's doc comment), so cooking to the same
   * path twice would just keep serving the first compile. Deletes any other generation's leftover
   * file for this same @p id before writing the new one, so at most one ever exists on disk per graph
   * even though several have existed in the run's shader_cache/pipeline_cache by then.
   */
  [[nodiscard]] static auto cook_shader_graph(const math::uuid& id, std::uint64_t generation, const shader_graph::create_info& create_info) -> bool;

private:

  static auto _cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  static auto _cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  static auto _cook_font(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  static auto _load_cooked_font(const std::filesystem::path& cooked, cooked_font_data& data) -> bool;

  /** @brief Computes tangents from scratch for vertices[vertex_start, vertex_start + vertex_count) of a primitive lacking TANGENT (normals/UVs must already be populated); indices are that primitive's triangle indices, offset by vertex_start. */
  /** @brief Computes flat-shaded, area-weighted vertex normals from scratch for vertices[vertex_start, vertex_start + vertex_count) of a primitive lacking NORMAL (positions must already be populated); indices are that primitive's triangle indices, offset by vertex_start. */
  static auto _generate_normals(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void;

  static auto _generate_tangents(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void;

  /** @brief Reorders a submesh's vertex/index slice in place for GPU cache efficiency (meshoptimizer's vertex-cache/overdraw/vertex-fetch trio), then derives a coarser LOD chain via meshopt_simplify, appending each level's indices to `indices`. */
  /** @brief @p skin_vertices, when non-null, is kept parallel to @p vertices through the same vertex-fetch reorder (see meshopt_optimizeVertexFetchRemap's "multiple vertex streams" note). */
  static auto _optimize_and_generate_lods(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count, std::vector<skin_vertex>* skin_vertices = nullptr) -> std::vector<mesh_lod>;

  static auto _cook_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options) -> bool;

  /** @brief @p animation_clip_original_indices comes back one entry per cooked clip, each clip's *original* position in the source's animation list (not a renumbered "Nth cooked" count) -- see derive_animation_clip_uuid's doc comment for why that distinction matters when only some clips were selected at cook time. */
  static auto _load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds, std::vector<skin_vertex>& skin_vertices, std::vector<std::uint32_t>& animation_clip_original_indices) -> bool;

  static auto _cook_material(const math::uuid& id, const material_description& description) -> bool;

  static auto _cook_skeleton(const math::uuid& id, const std::vector<skeleton::joint>& joints) -> bool;

  static auto _load_cooked_skeleton(const math::uuid& id, std::vector<skeleton::joint>& joints) -> bool;

  static auto _cook_animation_clip(const math::uuid& id, const animation_clip_data& data) -> bool;

  static auto _load_cooked_animation_clip(const math::uuid& id, animation_clip_data& data) -> bool;

}; // class asset_cooker

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_COOKER_HPP_
