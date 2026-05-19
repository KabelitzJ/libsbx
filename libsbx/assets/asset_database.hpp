// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSET_DATABASE_HPP_
#define LIBSBX_ASSETS_ASSET_DATABASE_HPP_

#include <filesystem>
#include <string>
#include <unordered_map>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset.hpp>

namespace sbx::assets {

/**
 * @brief Stores the canonical asset_record for every known asset and provides bidirectional lookup between UUID and source path.
 *
 * Does not own typed payloads. Does not track dependencies. Those layers sit above this one in the assets_module.
 */
class asset_database {

public:

  asset_database() = default;

  ~asset_database() = default;

  asset_database(const asset_database&) = delete;

  auto operator=(const asset_database&) -> asset_database& = delete;

  /**
   * @brief Inserts or replaces a record. The path index is updated to match the (possibly new) source path. Returns a reference to the stored record.
   */
  auto insert(asset_record record) -> asset_record&;

  /**
   * @brief Removes a record and its path-index entry. Returns true if a record was erased.
   */
  auto erase(const math::uuid& id) -> bool;

  auto contains(const math::uuid& id) const -> bool;

  auto contains_path(const std::filesystem::path& source) const -> bool;

  /**
   * @brief Returns the record for @p id.
   *
   * @throws sbx::utility::runtime_error if no record exists for @p id.
   */
  auto get(const math::uuid& id) -> asset_record&;

  auto get(const math::uuid& id) const -> const asset_record&;

  auto try_get(const math::uuid& id) -> memory::observer_ptr<asset_record>;

  auto try_get(const math::uuid& id) const -> memory::observer_ptr<const asset_record>;

  /**
   * @brief Returns the UUID bound to @p source, or math::uuid::nil() if none.
   */
  auto resolve(const std::filesystem::path& source) const -> math::uuid;

  /**
   * @brief Updates the source path of an existing record. Removes the old path-index entry and inserts the new one.
   *
   * @throws sbx::utility::runtime_error if no record exists for @p id.
   */
  auto rebind_path(const math::uuid& id, const std::filesystem::path& source) -> void;

  /**
   * @brief Increments the refcount. Returns the new value.
   */
  auto acquire(const math::uuid& id) -> std::uint32_t;

  /**
   * @brief Decrements the refcount (saturating at zero). Returns the new value.
   */
  auto release(const math::uuid& id) -> std::uint32_t;

  /**
   * @brief Bumps the generation counter (for hot-reload propagation). Returns the new value.
   */
  auto bump_generation(const math::uuid& id) -> std::uint32_t;

  auto records() const noexcept -> const std::unordered_map<math::uuid, asset_record>& {
    return _records;
  }

  auto size() const noexcept -> std::size_t {
    return _records.size();
  }

  auto clear() -> void;

private:

  static auto _normalize(const std::filesystem::path& source) -> std::string;

  auto _erase_path_entry(const std::filesystem::path& source) -> void;

  std::unordered_map<math::uuid, asset_record> _records;
  std::unordered_map<std::string, math::uuid> _by_path;

}; // class asset_database

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_DATABASE_HPP_
