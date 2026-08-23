// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_COOKER_HPP_
#define LIBSBX_ASSETS_ASSET_COOKER_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>

namespace sbx::assets {

/**
 * @brief Extracts every material a cooked mesh embeds into a standalone, editable `.material`
 * asset next to the mesh (reusing one already there rather than overwriting it) instead of a
 * hidden, unregistered cook-cache blob. On by default.
 */
struct mesh_import_options {
  bool extract_materials{true};
}; // struct mesh_import_options

/** @brief Decoded, GPU-independent pixel data — the shared shape resolve_texture/resolve_environment hand back. */
struct pixel_data {
  std::vector<std::byte> pixels;
  std::uint32_t width{0u};
  std::uint32_t height{0u};
}; // struct pixel_data

struct cooked_submesh {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  math::volume bounds;
  math::uuid material;
}; // struct cooked_submesh

struct cooked_mesh_data {
  std::vector<vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<cooked_submesh> submeshes;
  math::volume bounds;
}; // struct cooked_mesh_data

/** @brief A material's fields with texture *uuids*, not resolved handles — same shape whether it came from a glTF embed or a cooked blob. */
struct material_description {
  std::string name{"material"};
  math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
  math::vector3 emissive_factor{0.0f, 0.0f, 0.0f};
  std::float_t metallic_factor{1.0f};
  std::float_t roughness_factor{1.0f};
  alpha_mode alpha{alpha_mode::opaque};
  std::float_t alpha_cutoff{0.5f};
  bool is_double_sided{false};
  math::uuid albedo{math::uuid::nil()};
  math::uuid normal{math::uuid::nil()};
  math::uuid metallic_roughness{math::uuid::nil()};
  math::uuid occlusion{math::uuid::nil()};
  math::uuid emissive{math::uuid::nil()};
}; // struct material_description

/**
 * @brief Given an embedded glTF material and the mesh's own path, decides its uuid — either by
 * extracting it into a standalone, GPU-backed `.material` asset (residency's job) or some other
 * strategy the caller supplies. Only invoked when `mesh_import_options::extract_materials` is set;
 * the cooker has a pure, dependency-free fallback of its own otherwise.
 */
using material_resolver = std::function<math::uuid(const material_description&, const std::filesystem::path&)>;

/**
 * @brief Turns source assets (glTF meshes, images, HDR equirects, hand-authored `.material` YAML)
 * into versioned, on-disk cooked caches, and owns the asset database (uuid <-> path, the
 * import manifest, staleness tracking). Pure file I/O — no dependency on `graphics::` or any
 * GPU-side type; @ref asset_residency is what turns what this class produces into GPU-resident
 * resources.
 */
class asset_cooker final : public utility::noncopyable {

public:

  asset_cooker() = default;

  ~asset_cooker();

  /**
   * @brief Registers an asset by its path and returns its stable UUID.
   * @param path Must be resolvable from the current working directory — not merely relative to
   * the assets directory.
   */
  auto import(const std::filesystem::path& path) -> math::uuid;

  /**
   * @brief Imports every supported asset under a subdirectory.
   * @param root Must be resolvable from the current working directory. Empty (default) is *not*
   * "the whole assets tree" here — pass `project.assets_directory()` explicitly for that.
   */
  auto import_directory(const std::filesystem::path& root = {}) -> void;

  /** @brief The project-relative path an asset was imported from, or empty if unknown. */
  [[nodiscard]] auto path_of(const math::uuid& id) const -> std::filesystem::path;

  /** @brief Loads the manifest from disk on first call; a no-op after that. Idempotent. */
  auto ensure_manifest_loaded() -> void;

  [[nodiscard]] static auto absolute(const std::filesystem::path& relative) -> std::filesystem::path;

  [[nodiscard]] static auto relative(const std::filesystem::path& absolute) -> std::filesystem::path;

  /** @brief Staleness-gated cook + read. `id` must already be known (see @ref import). */
  [[nodiscard]] auto resolve_texture(const math::uuid& id) -> std::optional<pixel_data>;

  /** @brief Staleness-gated cook + read. `id` must already be known (see @ref import). */
  [[nodiscard]] auto resolve_environment(const math::uuid& id) -> std::optional<pixel_data>;

  /**
   * @brief Staleness-gated cook + read for a mesh. @p resolve_material is only invoked when
   * `options.extract_materials` is set — cooking with it off never calls it.
   */
  [[nodiscard]] auto resolve_mesh(const math::uuid& id, const mesh_import_options& options, const material_resolver& resolve_material) -> std::optional<cooked_mesh_data>;

  /** @brief Reads a material cooked as a side effect of a mesh import (not a hand-authored `.material` file). */
  [[nodiscard]] auto resolve_cooked_material(const math::uuid& id) -> std::optional<material_description>;

private:

  // One row of the asset manifest: the durable uuid -> source path index plus the staleness data
  // (content hash + cooker version) recorded at the last cook.
  struct manifest_entry {
    std::filesystem::path path{};      // project source path (same form as _paths)
    std::uint32_t cooker_version{0u};  // cooker that produced the current cooked output (0 = never)
    std::uint64_t source_hash{0u};     // source content hash at last cook
    std::int64_t source_mtime{0};      // source mtime at last cook (fast-path skip)
  }; // struct manifest_entry

  auto _read_or_create_meta(const std::filesystem::path& path) -> math::uuid;

  [[nodiscard]] auto _cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path;

  [[nodiscard]] auto _is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool;

  auto _record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void;

  [[nodiscard]] auto _manifest_path() const -> std::filesystem::path;

  auto _load_manifest() -> void;

  auto _save_manifest() -> void;

  auto _cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  auto _load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  auto _cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool;

  auto _load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool;

  [[nodiscard]] static auto _derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid;

  /**
   * @brief Fills in vertices[vertex_start, vertex_start + vertex_count)'s tangents from scratch,
   * for a glTF primitive that didn't provide its own TANGENT attribute (its normals and UVs must
   * already be populated). indices[index_start, index_start + index_count) are that primitive's
   * triangle indices, already offset by vertex_start.
   */
  static auto _generate_tangents(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void;

  auto _cook_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options, const material_resolver& resolve_material) -> bool;

  auto _load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds) -> bool;

  auto _cook_material(const math::uuid& id, const material_description& description) -> bool;

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, math::uuid> _uuids{};
  std::unordered_map<math::uuid, std::filesystem::path> _paths{}; // project-relative (to assets directory)

  std::unordered_map<math::uuid, manifest_entry> _manifest{};
  bool _manifest_loaded{false};
  bool _manifest_dirty{false};

}; // class asset_cooker

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_COOKER_HPP_
