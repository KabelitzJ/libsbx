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

#include <yaml-cpp/yaml.h>

#include <libsbx/assets/importer_registry.hpp>
#include <libsbx/assets/asset.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets { 

struct import_context {
  std::filesystem::path source;
  std::filesystem::path resolved;
  YAML::Node settings;
  math::uuid id;
}; // struct import_context

class importer_registry {

  template<typename>
  friend class importer;

  struct importer_base {
    virtual ~importer_base() = default;
    virtual auto type() const -> std::string_view = 0;
    virtual auto import(const import_context& context) -> std::unique_ptr<asset_base> = 0;
  }; // class importer_base

public:

  importer_registry() = default;

  ~importer_registry() = default;

  importer_registry(const importer_registry&) = delete;

  auto operator=(const importer_registry&) -> importer_registry& = delete;

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
  auto find_for(const std::filesystem::path& source) const -> std::shared_ptr<importer_base>;

  /**
   * @brief Returns the importer registered exactly for @p extension, or nullptr.
   */
  auto find(std::string_view extension) const -> std::shared_ptr<importer_base>;

  auto contains(std::string_view extension) const -> bool;

  auto size() const noexcept -> std::size_t {
    return _by_extension.size();
  }

  auto clear() -> void;

private:

  struct pending_importer {
    std::vector<std::string> extensions;
    std::function<std::shared_ptr<importer_base>()> factory;
  }; // struct pending_importer

  static auto _pending_importers() -> std::vector<pending_importer>& {
    static auto instance = std::vector<pending_importer>{};

    return instance;
  }

  static auto _normalize(std::string_view extension) -> std::string;

  auto _register_importer(std::string_view extension, std::shared_ptr<importer_base> instance) -> void;

  std::unordered_map<std::string, std::shared_ptr<importer_base>> _by_extension;

}; // class importer_registry

template<typename Derived>
class importer : public importer_registry::importer_base {

public:

  virtual ~importer() = default;

protected:

  using base_type = importer_registry::importer_base;

  static auto register_importer(std::initializer_list<std::string_view> extensions) -> bool {
    auto names = std::vector<std::string>{};

    names.reserve(extensions.size());

    for (const auto extension : extensions) {
      names.emplace_back(extension);
    }

    importer_registry::_pending_importers().push_back(importer_registry::pending_importer{
      .extensions = std::move(names),
      .factory = []() -> std::shared_ptr<importer_base> {
        return std::make_shared<Derived>();
      }
    });

    return true;
  }

}; // class importer

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_IMPORTER_REGISTRY_HPP_
