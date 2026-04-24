// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <filesystem>

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

template<utility::string_literal Type>
struct asset_handle {

  inline static constexpr auto hash = Type.hash();

  inline static constexpr auto invalid = std::uint32_t{0xFFFFFFFF};

  constexpr asset_handle()
  : _handle{invalid}, 
    _generation{0} { }

  constexpr asset_handle(const std::uint32_t handle, const std::uint32_t generation)
  : _handle{handle}, 
    _generation{generation} { }

  constexpr auto handle() const noexcept -> std::uint32_t {
    return _handle;
  }

  constexpr auto generation() const noexcept -> std::uint32_t {
    return _generation;
  }

  constexpr auto operator==(const asset_handle& other) const noexcept -> bool {
    return _handle == other._handle && _generation == other._generation;
  }

  constexpr auto is_valid() const noexcept -> bool {
    return _handle != invalid;
  }

  constexpr operator bool() const noexcept {
    return is_valid();
  }

private:

  std::uint32_t _handle;
  std::uint32_t _generation;

}; // struct asset_handle

struct asset_metadata {
  std::filesystem::path path;
  std::string name;
  std::string type{"unknown"};
  std::string source{"dynamic"};
}; // struct asset_metadata

struct asset_io {
  std::string name;
  std::function<void()> save; 
  std::function<void()> load; 
}; // struct asset_io

class asset_io_registry {

  struct scope { };

public:

  template<typename Type>
  using type_id = utility::scoped_type_id<scope, Type>;

  template<typename Type, std::invocable<const Type&> Save, std::invocable<void> Load>
  auto register_asset(const std::string& name, Save&& save, Load&& load) -> void {
    const auto id = assets::type_id<Type>::value();

    _by_name[name] = id;

    _by_id[id] = asset_io{
      .name = name,
      .save = [s = std::forward<Save>(save)]() -> void {
        std::invoke(s);
      },
      .load = []() -> void {

      }
    };
  }

  auto get(const std::uint32_t id) -> asset_io& {
    return _by_id.at(id);
  }

  auto has(const std::uint32_t id) -> bool {
    return _by_id.contains(id);
  }

  auto get(const std::string& name) -> asset_io& {
    return _by_id.at(_by_name.at(name));
  }

private:

  std::unordered_map<std::uint32_t, asset_io> _by_id;
  std::unordered_map<std::string, std::uint32_t> _by_name;

}; // class asset_io_registry

class assets_module : public core::module<assets_module> {

  inline static const auto is_registered = register_module(stage::post);

  inline static constexpr auto prefix = std::string_view{"res://"};

public:

  assets_module()
  : _thread_pool{std::thread::hardware_concurrency()},
    _asset_root{std::filesystem::current_path()} {
    utility::logger<"assets">::info("4cc of 'IMAG': {:#010x}", fourcc<"IMAG">());
  }

  ~assets_module() override {
    for (const auto& container : _containers) {
      container->clear();
    }
  }

  auto update() -> void override {
    
  }

  auto asset_root() const -> const std::filesystem::path& {
    return _asset_root;
  }

  auto set_asset_root(const std::filesystem::path& root) -> void {
    if (!std::filesystem::exists(root)) {
      throw utility::runtime_error{"New asset root path '{}' does not exist", root.string()};
    }

    utility::logger<"assets">::debug("Setting asset_root to '{}'", root.string());

    _asset_root = root;

    auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

    const auto alias = filesystem::alias{"res://"};

    if (filesystem_module.is_alias_registered(alias)) {
      filesystem_module.unregister_alias(alias);
    }

    filesystem_module.create_filesystem<filesystem::native_filesystem>(alias, root.generic_string());
  }

  auto resolve_path(const std::filesystem::path& path) -> std::filesystem::path {
    if (path.empty()) {
      return path;
    }

    auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();

    if (filesystem_module.is_alias_registered(filesystem::alias{"res://"})) {
      return filesystem_module.native_path_of(path);
    }

    const auto& path_string = path.string();

    if (path_string.starts_with(prefix)) {
      return _asset_root / path_string.substr(prefix.size());
    }

    return path;
  }

  template<typename Function, typename... Args>
  requires (std::is_invocable_v<Function, Args...>)
  auto submit(Function&& function, Args&&... args) -> std::future<std::invoke_result_t<Function, Args...>> {
    return _thread_pool.submit(std::forward<Function>(function), std::forward<Args>(args)...);
  }

  template<utility::string_literal Type, typename... Args>
  auto load_asset(const std::filesystem::path& path, Args&&... args) -> asset_handle<Type> {

  }

  template<typename Type, std::invocable<void> Save, std::invocable<void> Load>
  auto register_asset(const std::string& name, Save&& save, Load&& load) -> void {
    _asset_io_registry.register_asset<Type>(name, std::forward<Save>(save), std::forward<Load>(load));
  }

  auto asset_io(const std::uint32_t id) -> asset_io& {
    return _asset_io_registry.get(id);
  }

  auto has_asset_io(const std::uint32_t id) -> bool {
    return _asset_io_registry.has(id);
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

  thread_pool _thread_pool;
  std::filesystem::path _asset_root;

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

  asset_io_registry _asset_io_registry;

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
