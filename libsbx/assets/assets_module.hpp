// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <iterator>
#include <utility>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/alias.hpp>
#include <libsbx/filesystem/filesystem_module.hpp>
#include <libsbx/filesystem/native_filesystem.hpp>

#include <libsbx/assets/thread_pool.hpp>
#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/asset_database.hpp>
#include <libsbx/assets/serializer_registry.hpp>
#include <libsbx/assets/meta_file.hpp>

namespace sbx::assets {

class assets_module : public core::module<assets_module> {

  inline static const auto is_registered = register_module(stage::post, dependencies<filesystem::filesystem_module>{});

public:

  assets_module()
  : _thread_pool{std::thread::hardware_concurrency()} {
    _register_asset_root(std::filesystem::current_path());
    _serializers.install_registered();
  }

  ~assets_module() override {
    _payloads.clear();
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
   * @brief Synchronously resolves @p source to a stable UUID, importing the payload if it is not already loaded.
   *
   * @p source may carry a `#sub_id` fragment (e.g. `res://model.gltf#animation:Idle`) to address a single asset inside a container source. Without a fragment the source's primary asset is loaded.
   *
   * Identity is established from the .meta sidecar next to the resolved file; if no sidecar exists one is minted and written (best-effort, skipped on a read-only asset root). Sub-asset UUIDs are recorded in the sidecar so re-imports reuse them. Loading the same (source, sub_id) twice returns the same UUID and increments its reference count.
   *
   * @throws sbx::utility::runtime_error if no serializer produces the requested asset or the import fails.
   */
  auto load_asset(const std::filesystem::path& source, const YAML::Node& default_settings = YAML::Node{}) -> math::uuid {
    const auto [base, sub_id] = _split_fragment(source);

    if (const auto existing = _database.resolve(base, sub_id); existing != math::uuid::nil() && _database.get(existing).state == load_state::ready) {
      _database.acquire(existing);

      return existing;
    }

    const auto serializers = _serializers.find_all_for(base);

    if (serializers.empty()) {
      throw utility::runtime_error{"No serializer registered for asset '{}'", base.string()};
    }

    const auto resolved = resolve_path(base);
    const auto meta_path = meta_path_for(resolved);

    auto meta = read_meta_file(meta_path);
    auto meta_dirty = false;

    const auto settings = (meta && meta->import_settings && meta->import_settings.IsDefined() && !meta->import_settings.IsNull()) ? meta->import_settings : default_settings;

    const auto probe = serializer_context{.source = base, .resolved = resolved, .settings = settings, .id = math::uuid::nil(), .sub_id = sub_id};

    const auto owner = _serializers.owner_for(serializers, probe, sub_id);

    if (!owner) {
      throw utility::runtime_error{"No serializer produces sub-asset '{}' of '{}'", sub_id, base.string()};
    }

    if (!meta) {
      const auto primary = sub_id.empty() ? owner : _serializers.owner_for(serializers, probe, std::string_view{});
      const auto primary_type = primary ? std::string{primary->type()} : std::string{serializers.front()->type()};

      meta = meta_data{.id = math::uuid{}, .type = primary_type, .import_settings = default_settings};
      meta_dirty = true;
    }

    auto id = math::uuid::nil();
    auto type = std::string{};

    if (sub_id.empty()) {
      id = meta->id;
      type = meta->type;
    } else {
      type = std::string{owner->type()};

      if (const auto entry = meta->sub_assets.find(sub_id); entry != meta->sub_assets.end()) {
        id = entry->second;
      } else {
        id = math::uuid{};
        meta->sub_assets.emplace(sub_id, id);
        meta_dirty = true;
      }
    }

    if (meta_dirty) {
      try {
        write_meta_file(meta_path, *meta);
      } catch (const std::exception& error) {
        utility::logger<"assets">::warn("Could not write .meta for '{}': {}", base.string(), error.what());
      }
    }

    if (!_database.contains(id)) {
      _database.insert(asset_record{.id = id, .parent = sub_id.empty() ? math::uuid::nil() : meta->id, .type = type, .sub_id = sub_id, .source = base, .state = load_state::unloaded});
    }

    auto& record = _database.get(id);

    record.state = load_state::loading;

    const auto context = serializer_context{.source = base, .resolved = resolved, .settings = settings, .id = id, .sub_id = sub_id};

    try {
      _payloads[id] = owner->read(context);
    } catch (...) {
      record.state = load_state::failed;

      throw;
    }

    record.state = load_state::ready;

    _database.acquire(id);

    return id;
  }

  /**
   * @brief Loads a single asset addressed by (@p source, @p sub_id). Equivalent to appending `#sub_id` to @p source.
   */
  auto load_sub_asset(const std::filesystem::path& source, std::string_view sub_id, const YAML::Node& default_settings = YAML::Node{}) -> math::uuid {
    if (sub_id.empty()) {
      return load_asset(source, default_settings);
    }

    auto qualified = source.generic_string();

    qualified += '#';
    qualified += sub_id;

    return load_asset(qualified, default_settings);
  }

  /**
   * @brief Lists every asset (primary and sub-assets) that the registered serializers can extract from @p source.
   *
   * Does not import anything. Intended for the editor's importer UI.
   */
  auto enumerate_sub_assets(const std::filesystem::path& source) -> std::vector<sub_asset_info> {
    const auto [base, sub_id] = _split_fragment(source);

    static_cast<void>(sub_id);

    const auto serializers = _serializers.find_all_for(base);

    const auto resolved = resolve_path(base);

    const auto context = serializer_context{.source = base, .resolved = resolved, .settings = YAML::Node{}, .id = math::uuid::nil(), .sub_id = std::string{}};

    auto result = std::vector<sub_asset_info>{};

    for (const auto& serializer : serializers) {
      auto entries = serializer->enumerate(context);

      result.insert(result.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));
    }

    return result;
  }

  auto add_runtime_asset(std::unique_ptr<asset> payload) -> math::uuid {
    const auto id = math::uuid{};

    _database.insert(asset_record{.id = id, .source = {}, .state = load_state::ready});
    _database.acquire(id);

    _payloads[id] = std::move(payload);

    return id;
  }

  auto save_asset(const math::uuid& id, const std::filesystem::path& source) -> bool {
    const auto payload = _payloads.find(id);

    if (payload == _payloads.end()) {
      return false;
    }

    const auto resolved = resolve_path(source);

    const auto& type = _database.get(id).type;

    const auto serializer = _serializers.find_writer(resolved, type);

    if (!serializer) {
      return false;
    }

    const auto context = serializer_context{.source = source, .resolved = resolved, .settings = {}, .id = id, .sub_id = {}};

    if (!serializer->write(context, payload->second)) {
      return false;
    }

    _database.rebind_path(id, source);

    return true;
  }

  /**
   * @brief Re-writes an already-file-backed asset to its own source via the matching serializer.
   *
   * Used by the editor to persist in-place edits. Returns false if the asset isn't loaded, has no source, or its format has no writer.
   */
  auto save_asset(const math::uuid& id) -> bool {
    if (!_database.contains(id)) {
      return false;
    }

    const auto source = _database.get(id).source;

    if (source.empty()) {
      return false;
    }

    return save_asset(id, source);
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

  auto source_of(const math::uuid& id) const -> std::optional<std::filesystem::path> {
    if (const auto record = _database.try_get(id); record) {
      return record->source;
    }

    return std::nullopt;
  }

  /**
   * @brief Whether a payload is currently loaded for @p id.
   */
  auto is_loaded(const math::uuid& id) const -> bool {
    return _payloads.contains(id);
  }

  /**
   * @brief Returns the payload for @p id downcast to @p Type, or nullptr if not loaded or not that type.
   *
   * Non-throwing counterpart to get_loaded; use when the caller dispatches on type.
   */
  template<typename Type>
  auto try_get_loaded(const math::uuid& id) -> Type* {
    const auto entry = _payloads.find(id);

    if (entry == _payloads.end()) {
      return nullptr;
    }

    return dynamic_cast<Type*>(entry->second.get());
  }

  auto serializers() -> serializer_registry& {
    return _serializers;
  }

private:

  static auto _split_fragment(const std::filesystem::path& source) -> std::pair<std::filesystem::path, std::string> {
    const auto text = source.generic_string();

    const auto hash = text.find('#');

    if (hash == std::string::npos) {
      return {source, std::string{}};
    }

    return {std::filesystem::path{text.substr(0u, hash)}, text.substr(hash + 1u)};
  }

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
  serializer_registry _serializers;
  std::unordered_map<math::uuid, std::unique_ptr<asset>> _payloads;

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
