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

#include <libsbx/assets/asset.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

/**
 * @brief Identifies one asset a serializer can extract from a source file.
 *
 * @ref sub_id is a stable, source-local name for the asset ("" for the source's primary asset, "animation:Idle", "skinned_mesh:0", ...). @ref type is the string type tag the extracted payload will carry.
 */
struct sub_asset_info {
  std::string sub_id;
  std::string type;
}; // struct sub_asset_info

struct serializer_context {
  std::filesystem::path source;
  std::filesystem::path resolved;
  YAML::Node settings;
  math::uuid id;
  std::string sub_id;
}; // struct serializer_context

class serializer_registry {

  template<typename>
  friend class serializer;

  struct serializer_base {
    virtual ~serializer_base() = default;

    virtual auto type() const -> std::string_view = 0;

    /**
     * @brief Lists every asset this serializer can extract from @p context.source.
     *
     * The default implementation reports a single primary asset (empty sub_id) tagged with type(). Serializers for container formats (glTF, FBX) override this to fan a source out into several assets.
     */
    virtual auto enumerate(const serializer_context& context) -> std::vector<sub_asset_info> {
      static_cast<void>(context);

      return {sub_asset_info{.sub_id = std::string{}, .type = std::string{type()}}};
    }

    /**
     * @brief Whether this serializer produces @p sub_id for @p context.source.
     *
     * The default claims only the primary (empty) sub_id. Overrides must agree with enumerate().
     */
    virtual auto owns(const serializer_context& context, std::string_view sub_id) -> bool {
      static_cast<void>(context);

      return sub_id.empty();
    }

    virtual auto read(const serializer_context& context) -> std::unique_ptr<asset> = 0;

    virtual auto write(const serializer_context& context, const std::unique_ptr<asset>& asset) -> bool = 0;
  }; // struct serializer_base

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
   * @brief Unregisters every serializer bound to a single extension. Returns true if a binding was removed.
   */
  auto unregister(std::string_view extension) -> bool;

  /**
   * @brief Returns every serializer whose registered extension matches the longest suffix of @p source's filename, in registration order. Empty if no match.
   *
   * For `foo.material.yaml`, lookup tries `.material.yaml` first, then `.yaml`.
   */
  auto find_all_for(const std::filesystem::path& source) const -> std::span<const std::shared_ptr<serializer_base>>;

  /**
   * @brief Returns the first serializer matching @p source, or nullptr. Convenience for single-asset formats.
   */
  auto find_for(const std::filesystem::path& source) const -> std::shared_ptr<serializer_base>;

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

  auto _matched_extension(const std::filesystem::path& source) const -> const std::vector<std::shared_ptr<serializer_base>>*;

  auto _register_serializer(std::string_view extension, std::shared_ptr<serializer_base> instance) -> void;

  std::unordered_map<std::string, std::vector<std::shared_ptr<serializer_base>>> _by_extension;

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
