// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_
#define LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <libsbx/assets/importer.hpp>

namespace sbx::assets {

namespace detail {

/**
 * @brief A deferred importer registration collected at static-init time.
 *
 * The factory is invoked once, when the assets_module installs the registered importers; the extensions are then bound to that instance.
 */
struct pending_importer {
  std::vector<std::string> extensions;
  std::function<std::shared_ptr<importer>()> factory;
}; // struct pending_importer

/**
 * @brief Process-wide list of importers registered via register_importer<T>(...).
 *
 * Function-local static so it is initialized on first use, avoiding static-init order issues between importer translation units.
 */
auto pending_importers() -> std::vector<pending_importer>&;

} // namespace detail

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
   * @brief Instantiates and binds every importer registered via register_importer<T>(...).
   *
   * Called once by the assets_module on construction. Safe to call again; later calls re-bind the same extensions.
   */
  auto install_registered() -> void;

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

/**
 * @brief Registers @p Derived as an importer for @p extensions, to be instantiated when the assets_module installs importers.
 *
 * Call this from a namespace-scope object in the importer's .cpp so the type is complete and the translation unit is linked:
 *
 *   namespace { 
 *     const auto registered = sbx::assets::register_importer<my_importer>({".ext"}); 
 *   }
 *
 * @tparam Derived A concrete importer type with a default constructor that does not touch other modules.
 */
template<typename Derived>
auto register_importer(std::initializer_list<std::string_view> extensions) -> bool {
  auto names = std::vector<std::string>{};

  names.reserve(extensions.size());

  for (const auto extension : extensions) {
    names.emplace_back(extension);
  }

  detail::pending_importers().push_back(detail::pending_importer{
    .extensions = std::move(names),
    .factory = []() -> std::shared_ptr<importer> {
      return std::make_shared<Derived>();
    }
  });

  return true;
}

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_
