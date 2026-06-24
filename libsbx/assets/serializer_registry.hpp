// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_SERIALIZER_REGISTRY_HPP_
#define LIBSBX_ASSETS_SERIALIZER_REGISTRY_HPP_

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

#include <libsbx/assets/serializer_registry.hpp>
#include <libsbx/assets/asset.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets { 

struct serializer_context {
  std::filesystem::path source;
  std::filesystem::path resolved;
  YAML::Node settings;
  math::uuid id;
}; // struct serializer_context

class serializer_registry {

  template<typename>
  friend class serializer;

  struct serializer_base {
    virtual ~serializer_base() = default;
    virtual auto type() const -> std::string_view = 0;
    virtual auto read(const serializer_context& context) -> std::unique_ptr<asset_base> = 0;
    virtual auto write(const serializer_context& context, const std::unique_ptr<asset_base>& asset) -> bool = 0;
  }; // class serializer_base

public:

  serializer_registry() = default;

  ~serializer_registry() = default;

  serializer_registry(const serializer_registry&) = delete;

  auto operator=(const serializer_registry&) -> serializer_registry& = delete;

  /**
   * @brief Instantiates and binds every serializer registered via register_serializer<T>(...).
   *
   * Called once by the assets_module on construction. Safe to call again; later calls re-bind the same extensions.
   */
  auto install_registered() -> void;

  /**
   * @brief Unregisters a single extension binding. Returns true if a binding was removed.
   */
  auto unregister(std::string_view extension) -> bool;

  /**
   * @brief Finds the serializer whose registered extension matches the longest suffix of @p source's filename. Returns nullptr if no match.
   *
   * For `foo.material.yaml`, lookup tries `.material.yaml` first, then `.yaml`.
   */
  auto find_for(const std::filesystem::path& source) const -> std::shared_ptr<serializer_base>;

  /**
   * @brief Returns the serializer registered exactly for @p extension, or nullptr.
   */
  auto find(std::string_view extension) const -> std::shared_ptr<serializer_base>;

  auto contains(std::string_view extension) const -> bool;

  auto size() const noexcept -> std::size_t {
    return _by_extension.size();
  }

  auto clear() -> void;

private:

  struct pending_serializer {
    std::vector<std::string> extensions;
    std::function<std::shared_ptr<serializer_base>()> factory;
  }; // struct pending_serializer

  static auto _pending_serializers() -> std::vector<pending_serializer>& {
    static auto instance = std::vector<pending_serializer>{};

    return instance;
  }

  static auto _normalize(std::string_view extension) -> std::string;

  auto _register_serializer(std::string_view extension, std::shared_ptr<serializer_base> instance) -> void;

  std::unordered_map<std::string, std::shared_ptr<serializer_base>> _by_extension;

}; // class serializer_registry

template<typename Derived>
class serializer : public serializer_registry::serializer_base {

public:

  virtual ~serializer() = default;

protected:

  using base_type = serializer_registry::serializer_base;

  static auto register_serializer(std::initializer_list<std::string_view> extensions) -> bool {
    auto names = std::vector<std::string>{};

    names.reserve(extensions.size());

    for (const auto extension : extensions) {
      names.emplace_back(extension);
    }

    serializer_registry::_pending_serializers().push_back(serializer_registry::pending_serializer{
      .extensions = std::move(names),
      .factory = []() -> std::shared_ptr<serializer_base> {
        return std::make_shared<Derived>();
      }
    });

    return true;
  }

}; // class serializer

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SERIALIZER_REGISTRY_HPP_
