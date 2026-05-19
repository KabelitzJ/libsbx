// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_
#define LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <libsbx/assets/importer.hpp>

namespace sbx::assets {

/**
 * @brief Maps file-extension suffixes to importer instances.
 *
 * Multi-segment suffixes (e.g. `.material.yaml`) are matched before single-segment ones (`.yaml`). All keys are normalized to lowercase and must include the leading dot.
 *
 * Importers are held by shared_ptr so a single instance can serve multiple extensions (e.g. one texture_importer for `.png`, `.jpg`, `.tga`).
 */
class importer_registry {

public:

  importer_registry() = default;

  ~importer_registry() = default;

  importer_registry(const importer_registry&) = delete;

  auto operator=(const importer_registry&) -> importer_registry& = delete;

  /**
   * @brief Registers @p instance for the given extension. Replaces any existing binding for that extension.
   *
   * @param extension Full suffix including the leading dot (e.g. `.png`, `.material.yaml`). Case-insensitive.
   */
  auto register_for(std::string_view extension, std::shared_ptr<importer> instance) -> void;

  /**
   * @brief Convenience overload registering one importer for multiple extensions.
   */
  auto register_for(std::initializer_list<std::string_view> extensions, std::shared_ptr<importer> instance) -> void;

  /**
   * @brief Unregisters a single extension binding. Returns true if a binding was removed.
   */
  auto unregister(std::string_view extension) -> bool;

  /**
   * @brief Finds the importer whose registered extension matches the longest suffix of @p source's filename. Returns nullptr if no match.
   *
   * For `foo.material.yaml`, lookup tries `.material.yaml` first, then `.yaml`.
   */
  auto find_for(const std::filesystem::path& source) const -> std::shared_ptr<importer>;

  /**
   * @brief Returns the importer registered exactly for @p extension, or nullptr.
   */
  auto find(std::string_view extension) const -> std::shared_ptr<importer>;

  auto contains(std::string_view extension) const -> bool;

  auto size() const noexcept -> std::size_t {
    return _by_extension.size();
  }

  auto clear() -> void;

private:

  static auto _normalize(std::string_view extension) -> std::string;

  std::unordered_map<std::string, std::shared_ptr<importer>> _by_extension;

}; // class importer_registry

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_
