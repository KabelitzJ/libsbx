// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_MANIFEST_HPP_
#define LIBSBX_ASSETS_ASSET_MANIFEST_HPP_

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

/**
 * @brief The asset database: uuid <-> source path, plus what was cooked (content hash and cooker version) for staleness checks.
 *
 * Main thread only and deliberately lock-free: asset_residency resolves paths and staleness up front and hands only the results to asset_loader's thread.
 */
class asset_manifest final : public utility::noncopyable {

public:

  asset_manifest() = default;

  ~asset_manifest();

  /**
   * @brief Registers an asset by path.
   *
   * @param path A cwd-resolvable path, not assets-relative.
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
   * @brief The project-relative path an asset was imported from.
   *
   * @param id The asset's uuid.
   *
   * @return The path, or empty if unknown.
   */
  [[nodiscard]] auto path_of(const math::uuid& id) const -> std::filesystem::path;

  /** @brief Loads the manifest from disk on the first call; a no-op afterwards. */
  auto ensure_loaded() -> void;

  [[nodiscard]] static auto absolute(const std::filesystem::path& relative) -> std::filesystem::path;

  [[nodiscard]] static auto relative(const std::filesystem::path& absolute) -> std::filesystem::path;

  /**
   * @brief Where an asset's cooked blob lives, whether or not it exists yet.
   *
   * @param id The asset's uuid.
   * @param extension The cooked file extension.
   *
   * @return The cooked path.
   */
  [[nodiscard]] auto cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path;

  /** @brief Whether `cooked` must be (re)produced: it's missing, the cooker version changed, or the source changed since the last cook. */
  [[nodiscard]] auto is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool;

  /**
   * @brief Records and persists that `id` was just cooked at `cooker_version` from `source`'s current content.
   *
   * @param id The asset's uuid.
   * @param cooker_version The cooker version used.
   * @param source The source file.
   */
  auto record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void;

  /**
   * @brief Moves a manifested file or directory on disk, keeping uuids by moving `.meta` sidecars along.
   *
   * @param old_path The cwd-resolvable source path.
   * @param new_path The cwd-resolvable destination path.
   * @param moved_assets Receives an assets-relative (old, new) pair for every manifested file that moved.
   *
   * @return False, with nothing touched, if the filesystem rename failed.
   */
  auto move(const std::filesystem::path& old_path, const std::filesystem::path& new_path, std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& moved_assets) -> bool;

  /**
   * @brief Deletes a manifested file or directory with its `.meta` and drops its entries. Cooked blobs are left as orphans.
   *
   * @param path The cwd-resolvable path to delete.
   */
  auto remove(const std::filesystem::path& path) -> void;

private:

  // Durable uuid -> source path index plus the staleness data recorded at the last cook.
  struct manifest_entry {
    std::filesystem::path path{};      // project source path (same form as _paths)
    std::uint32_t cooker_version{0u};  // cooker that produced the cooked output (0 = never)
    std::uint64_t source_hash{0u};     // source content hash at last cook
    std::int64_t source_mtime{0};      // source mtime at last cook, for the fast path
  }; // struct manifest_entry

  auto _read_or_create_meta(const std::filesystem::path& path) -> math::uuid;

  [[nodiscard]] auto _manifest_path() const -> std::filesystem::path;

  auto _load_manifest() -> void;

  auto _save_manifest() -> void;

  std::unordered_map<std::string, math::uuid> _uuids{};
  std::unordered_map<math::uuid, std::filesystem::path> _paths{}; // relative to the assets directory

  std::unordered_map<math::uuid, manifest_entry> _manifest{};
  bool _manifest_loaded{false};
  bool _manifest_dirty{false};

}; // class asset_manifest

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_MANIFEST_HPP_
