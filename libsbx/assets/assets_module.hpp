// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <filesystem>
#include <string>
#include <string_view>
#include <initializer_list>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/compression.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/type_id.hpp>
#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/string_literal.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/alias.hpp>
#include <libsbx/filesystem/filesystem_module.hpp>
#include <libsbx/filesystem/native_filesystem.hpp>

#include <libsbx/assets/thread_pool.hpp>
#include <libsbx/assets/metadata.hpp>
#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/asset_database.hpp>
#include <libsbx/assets/importer.hpp>
#include <libsbx/assets/importer_registry.hpp>
#include <libsbx/assets/meta_file.hpp>

namespace sbx::assets {

namespace detail {

struct assets_type_id_scope { };

} // namespace detail

template<utility::string_literal String>
requires (String.size() == 4u)
constexpr auto fourcc() -> std::uint32_t {
  return String[0] | (String[1] << 8) | (String[2] << 16) | (String[3] << 24);
}

/**
 * @brief A scoped type ID generator for the libsbx-assets scope.
 *
 * @tparam Type The type for which the ID is generated.
 */
template<typename Type>
using type_id = utility::scoped_type_id<detail::assets_type_id_scope, Type>;

class assets_module : public core::module<assets_module> {

  inline static const auto is_registered = register_module(stage::post, dependencies<filesystem::filesystem_module>{});

public:

  assets_module()
  : _thread_pool{std::thread::hardware_concurrency()} {
    _register_asset_root(std::filesystem::current_path());
  }

  ~assets_module() override {
    _payloads.clear();

    for (const auto& container : _containers) {
      container->clear();
    }
  }

  auto update() -> void override {

  }

  auto asset_root() const -> std::filesystem::path {
    auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

    return filesystem_module.native_path_of(std::string{"res://"});
  }

  auto set_asset_root(const std::filesystem::path& root) -> void {
    if (!std::filesystem::exists(root)) {
      throw utility::runtime_error{"New asset root path '{}' does not exist", root.string()};
    }

    utility::logger<"assets">::debug("Setting asset_root to '{}'", root.string());

    _register_asset_root(root);
  }

  auto resolve_path(const std::filesystem::path& path) -> std::filesystem::path {
    auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

    return filesystem_module.native_path_of(path);
  }

  template<typename Function, typename... Args>
  requires (std::is_invocable_v<Function, Args...>)
  auto submit(Function&& function, Args&&... args) -> std::future<std::invoke_result_t<Function, Args...>> {
    return _thread_pool.submit(std::forward<Function>(function), std::forward<Args>(args)...);
  }

  /**
   * @brief Registers @p instance for a single file-extension suffix (e.g. ".png").
   */
  auto register_importer(std::string_view extension, std::shared_ptr<importer> instance) -> void {
    _importers.register_for(extension, std::move(instance));
  }

  /**
   * @brief Registers @p instance for several file-extension suffixes at once.
   */
  auto register_importer(std::initializer_list<std::string_view> extensions, std::shared_ptr<importer> instance) -> void {
    _importers.register_for(extensions, std::move(instance));
  }

  /**
   * @brief Synchronously resolves @p source to a stable UUID, importing the payload if it is not already loaded.
   *
   * Identity is established from the .meta sidecar next to the resolved file; if no sidecar exists one is minted and written (best-effort, skipped on a read-only asset root). Loading the same source twice returns the same UUID and increments its reference count.
   *
   * @throws sbx::utility::runtime_error if no importer is registered for the source's extension or the import fails.
   */
  auto load_asset(const std::filesystem::path& source, const YAML::Node& default_settings = YAML::Node{}) -> math::uuid {
    const auto resolved = resolve_path(source);

    auto id = _database.resolve(source);

    if (id != math::uuid::nil() && _database.get(id).state == load_state::ready) {
      _database.acquire(id);

      return id;
    }

    const auto instance = _importers.find_for(source);

    if (!instance) {
      throw utility::runtime_error{"No importer registered for asset '{}'", source.string()};
    }

    auto settings = YAML::Node{};

    if (id == math::uuid::nil()) {
      auto type = std::string{instance->type()};

      if (const auto meta = read_meta_file(meta_path_for(resolved)); meta) {
        id = meta->id;
        type = meta->type;
        settings = meta->import_settings;
      } else {
        id = math::uuid{};
        settings = default_settings;

        try {
          write_meta_file(meta_path_for(resolved), meta_data{.id = id, .type = type, .import_settings = default_settings});
        } catch (const std::exception& error) {
          utility::logger<"assets">::warn("Could not write .meta for '{}': {}", source.string(), error.what());
        }
      }

      _database.insert(asset_record{.id = id, .type = type, .source = source, .state = load_state::unloaded});
    } else if (const auto meta = read_meta_file(meta_path_for(resolved)); meta) {
      settings = meta->import_settings;
    }

    auto& record = _database.get(id);

    record.state = load_state::loading;

    const auto context = import_context{.source = source, .resolved = resolved, .settings = settings, .id = id};

    try {
      _payloads[id] = instance->import(context);
    } catch (...) {
      record.state = load_state::failed;

      throw;
    }

    record.state = load_state::ready;

    _database.acquire(id);

    return id;
  }

  /**
   * @brief Returns the imported payload for @p id downcast to @p Type.
   *
   * @throws sbx::utility::runtime_error if no payload is loaded for @p id or it is not a @p Type.
   */
  template<typename Type>
  auto get_loaded(const math::uuid& id) -> Type& {
    const auto entry = _payloads.find(id);

    if (entry == _payloads.end()) {
      throw utility::runtime_error{"Asset with ID '{}' is not loaded", id};
    }

    auto* typed = dynamic_cast<Type*>(entry->second.get());

    if (!typed) {
      throw utility::runtime_error{"Asset with ID '{}' is not of the requested type", id};
    }

    return *typed;
  }

  /**
   * @brief Drops one reference to @p id. When the count reaches zero the payload is released and the record marked unloaded.
   *
   * @note GPU-backed payloads are freed immediately here; deferring this until the frame fence is a later step.
   */
  auto release(const math::uuid& id) -> void {
    if (!_database.contains(id)) {
      return;
    }

    const auto count = _database.release(id);

    if (count != 0u) {
      return;
    }

    auto node = _payloads.extract(id);

    _database.get(id).state = load_state::unloaded;

    if (node) {
      for (const auto& dependency : node.mapped()->dependencies()) {
        release(dependency);
      }
    }
  }

  template<typename Type, typename... Args>
  auto add_asset(Args&&... args) -> math::uuid {
    const auto id = math::uuid{};
    const auto type = type_id<Type>::value();

    if (type >= _containers.size()) {
      _containers.resize(std::max(_containers.size(), static_cast<std::size_t>(type + 1u)));
    }

    if (!_containers[type]) {
      _containers[type] = std::make_unique<container<Type>>();
    }

    static_cast<container<Type>*>(_containers[type].get())->add(id, std::forward<Args>(args)...);

    return id;
  }

  template<typename Type>
  auto add_asset(std::unique_ptr<Type>&& asset) -> math::uuid {
    const auto id = math::uuid{};
    const auto type = type_id<Type>::value();

    if (type >= _containers.size()) {
      _containers.resize(std::max(_containers.size(), static_cast<std::size_t>(type + 1u)));
    }

    if (!_containers[type]) {
      _containers[type] = std::make_unique<container<Type>>();
    }

    static_cast<container<Type>*>(_containers[type].get())->add(id, std::move(asset));

    return id;
  }

  template<typename Type>
  auto get_asset(const math::uuid& id) const -> const Type& {
    const auto type = type_id<Type>::value();

    if (type >= _containers.size() || !_containers[type]) {
      throw std::runtime_error{"Asset does not exist"};
    }

    return static_cast<const container<Type>*>(_containers[type].get())->get(id);
  }

  template<typename Type>
  auto get_asset(const math::uuid& id) -> Type& {
    const auto type = type_id<Type>::value();

    if (type >= _containers.size() || !_containers[type]) {
      throw std::runtime_error{"Asset does not exist"};
    }

    return static_cast<container<Type>*>(_containers[type].get())->get(id);
  }

private:

  auto _register_asset_root(const std::filesystem::path& root) -> void {
    auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

    const auto alias = filesystem::alias{"res://"};

    if (filesystem_module.is_alias_registered(alias)) {
      filesystem_module.unregister_alias(alias);
    }

    filesystem_module.create_filesystem<filesystem::native_filesystem>(alias, root.generic_string());
  }

  thread_pool _thread_pool;

  asset_database _database;
  importer_registry _importers;
  std::unordered_map<math::uuid, std::unique_ptr<asset_base>> _payloads;

  struct container_base {
    virtual ~container_base() = default;
    virtual auto remove(const math::uuid& id) -> void = 0;
    virtual auto clear() -> void = 0;
  };

  template<typename Type>
  class container : public container_base {

  public:

    container() {

    }

    ~container() override {

    }

    auto remove(const math::uuid& id) -> void override {
      _assets.erase(id);
    }

    auto clear() -> void override {
      _assets.clear();
    }

    template<typename... Args>
    auto add(const math::uuid& id, Args&&... args) -> void {
      _assets.insert({id, std::make_unique<Type>(std::forward<Args>(args)...)});
    }

    auto add(const math::uuid& id, std::unique_ptr<Type>&& asset) -> void {
      _assets.insert({id, std::move(asset)});
    }

    auto get(const math::uuid& id) const -> const Type& {
      const auto entry = _assets.find(id);

      if (entry == _assets.end()) {
        throw utility::runtime_error{"Asset with ID '{}' not found", id};
      }

      return *entry->second;
    }

    auto get(const math::uuid& id) -> Type& {
      auto entry = _assets.find(id);

      if (entry == _assets.end()) {
        throw utility::runtime_error{"Asset with ID '{}' not found", id};
      }

      return *entry->second;
    }

  private:

    std::unordered_map<math::uuid, std::unique_ptr<Type>> _assets;

  };

  std::vector<std::unique_ptr<container_base>> _containers;

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
