// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_VIRTUAL_FILESYSTEM_HPP_
#define LIBSBX_FILESYSTEM_VIRTUAL_FILESYSTEM_HPP_

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <algorithm>
#include <optional>
#include <functional>
#include <filesystem>

#include <libsbx/filesystem/filesystem.hpp>
#include <libsbx/filesystem/file_base.hpp>
#include <libsbx/filesystem/alias.hpp>

namespace sbx::filesystem {

class virtual_filesystem final {

public:

  using filesystem_list = std::vector<filesystem_ptr>;
  using filesystem_map = std::unordered_map<alias, filesystem_list>;

  virtual_filesystem() = default;

  ~virtual_filesystem() {
    auto lock = std::scoped_lock{_mutex};

    for (auto& [alias, list] : _filesystems) {
      for (auto& filesystem : list) {
        if (filesystem) {
          filesystem->shutdown();
        }
      }
    }
  }

  void add_filesystem(const alias& alias, const filesystem_ptr& filesystem) {
    if (!filesystem) {
      return;
    }

    auto lock = std::scoped_lock{_mutex};

    _filesystems[alias].push_back(filesystem);

    if (std::find(_sorted_alias.begin(), _sorted_alias.end(), alias) == _sorted_alias.end()) {
      _sorted_alias.push_back(alias);
    }

    std::sort(_sorted_alias.begin(), _sorted_alias.end(), [](const filesystem::alias& lhs, const filesystem::alias& rhs) {
      return lhs.length() > rhs.length();
    });
  }

  void add_filesystem(std::string alias, const filesystem_ptr& filesystem) {
    add_filesystem(filesystem::alias{std::move(alias)}, filesystem);
  }

  template<typename Type, typename... Args>
  [[nodiscard]] auto create_filesystem(const alias& alias, Args&&... args) -> std::shared_ptr<Type> {
    auto filesystem = std::make_shared<Type>(alias.string(), std::forward<Args>(args)...);

    if (!filesystem || !filesystem->initialize()) {
      return nullptr;
    }

    add_filesystem(alias, filesystem);

    return filesystem;
  }

  template<typename Type, typename... Args>
  [[nodiscard]] auto create_filesystem(std::string alias, Args&&... args) -> std::shared_ptr<Type> {
    return create_filesystem<Type>(filesystem::alias{std::move(alias)}, std::forward<Args>(args)...);
  }

  auto remove_filesystem(const alias& alias, const filesystem_ptr& filesystem) -> void {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _filesystems.find(alias);

    if (entry == _filesystems.end()) {
      return;
    }

    auto& list = entry->second;

    list.erase(std::remove(list.begin(), list.end(), filesystem), list.end());

    if (list.empty()) {
      _filesystems.erase(entry);

      _sorted_alias.erase(std::remove(_sorted_alias.begin(), _sorted_alias.end(), alias), _sorted_alias.end());
    }
  }

  auto remove_filesystem(std::string alias, const filesystem_ptr& filesystem) -> void {
    remove_filesystem(filesystem::alias{std::move(alias)}, filesystem);
  }

  [[nodiscard]] auto has_filesystem(const alias& alias, const filesystem_ptr& filesystem) const -> bool {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _filesystems.find(alias);

    if (entry == _filesystems.end()) {
      return false;
    }

    const auto& list = entry->second;

    return std::find(list.begin(), list.end(), filesystem) != list.end();
  }

  [[nodiscard]] auto has_filesystem(std::string alias, const filesystem_ptr& filesystem) const -> bool {
    return has_filesystem(filesystem::alias{std::move(alias)}, filesystem);
  }

  void unregister_alias(const alias& alias) {
    auto lock = std::scoped_lock{_mutex};

    _filesystems.erase(alias);
  
    _sorted_alias.erase(std::remove(_sorted_alias.begin(), _sorted_alias.end(), alias), _sorted_alias.end());
  }

  void unregister_alias(std::string alias) {
    unregister_alias(filesystem::alias{std::move(alias)});
  }

  [[nodiscard]] auto is_alias_registered(const alias& alias) const -> bool {
    auto lock = std::scoped_lock{_mutex};

    return _filesystems.find(alias) != _filesystems.end();
  }

  [[nodiscard]] auto is_alias_registered(std::string alias) const -> bool {
    return is_alias_registered(filesystem::alias{std::move(alias)});
  }

  [[nodiscard]] auto filesystems(const alias& alias) -> std::optional<std::reference_wrapper<const filesystem_list>> {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _filesystems.find(alias);

    if (entry != _filesystems.end()) {
      return std::cref(entry->second);
    }

    return std::nullopt;
  }

  [[nodiscard]] auto filesystems(std::string alias) -> std::optional<std::reference_wrapper<const filesystem_list>> {
    return filesystems(filesystem::alias{std::move(alias)});
  }

  [[nodiscard]] auto open_file(const std::string& path, file_base::mode mode) -> file_ptr {
    auto lock = std::scoped_lock{_mutex};

    auto result = _visit(path, [&](const filesystem_ptr& filesystem, [[maybe_unused]] auto is_main) -> std::optional<file_ptr> {
      if (filesystem->exists(path)) {
        if (auto file = filesystem->open_file(path, mode); file) {
          return file;
        }
      } else if (!filesystem->is_read_only() && file_base::mode_has_flag(mode, file_base::mode::write)) {
        if (auto file = filesystem->open_file(path, mode); file) {
          return file;
        }
      }

      return std::nullopt;
    });

    return result.value_or(nullptr);
  }

  [[nodiscard]] auto create_file(const std::string& path) -> file_ptr {
    auto lock = std::scoped_lock{_mutex};

    auto result = _visit(path, [&](const filesystem_ptr& filesystem, [[maybe_unused]] auto is_main) -> std::optional<file_ptr> {
      if (!filesystem->is_read_only()) {
        if (auto file = filesystem->create_file(path); file) {
          return file;
        }
      }

      return std::nullopt;
    });

    return result.value_or(nullptr);
  }

  [[nodiscard]] auto exists(const std::string& path) const -> bool {
    auto lock = std::scoped_lock{_mutex};

    auto result = _visit(path, [&](const filesystem_ptr& filesystem, [[maybe_unused]] auto is_main) -> std::optional<bool> {
      if (filesystem->exists(path)) {
        return true;
      }

      return std::nullopt;
    });

    return result.value_or(false);
  }

  [[nodiscard]] auto all_files() const -> std::vector<std::string> {
    auto lock = std::scoped_lock{_mutex};

    auto result = std::vector<std::string>{};
    auto seen = std::unordered_set<std::string>{};

    for (const auto& alias : _sorted_alias) {
      auto entry = _filesystems.find(alias);

      if (entry == _filesystems.end()) {
        continue;
      }

      const auto& list = entry->second;

      for (auto entry = list.rbegin(); entry != list.rend(); ++entry) {
        auto filesystem = *entry;
        const auto& files = filesystem->files();

        for (const auto& info : files) {
          const auto& path = info.virtual_path();

          if (seen.emplace(path).second) {
            result.push_back(path);
          }
        }
      }
    }

    std::sort(result.begin(), result.end());

    return result;
  }

  [[nodiscard]] auto native_path_of(const std::string& virtual_path) const -> std::filesystem::path {
    auto lock = std::scoped_lock{_mutex};

    for (const auto& alias : _sorted_alias) {
      const auto& prefix = alias.string();

      if (!virtual_path.starts_with(prefix)) {
        continue;
      }

      auto entry = _filesystems.find(alias);

      if (entry == _filesystems.end() || entry->second.empty()) {
        continue;
      }

      const auto& base = entry->second.back()->base_path();
      const auto tail = std::string_view{virtual_path}.substr(prefix.size());

      auto result = std::filesystem::path{base};

      if (!tail.empty()) {
        result /= std::string{tail};
      }

      return result;
    }

    return std::filesystem::path{virtual_path};
  }

  [[nodiscard]] auto native_path_of(const std::filesystem::path& virtual_path) const -> std::filesystem::path {
    return native_path_of(virtual_path.string());
  }

private:

  template<typename Callback>
  requires (std::is_invocable_v<Callback, const filesystem_ptr&, bool>)
  auto _visit(const std::string& path, Callback&& callback) const -> std::invoke_result_t<Callback, const filesystem_ptr&, bool> {
    using result_type = std::invoke_result_t<Callback, const filesystem_ptr&, bool>;

    for (const auto& alias : _sorted_alias) {
      if (!path.starts_with(alias.string())) {
        continue;
      }

      auto entry = _filesystems.find(alias);

      if (entry == _filesystems.end()) {
        continue;
      }

      const auto& list = entry->second;

      for (auto entry = list.rbegin(); entry != list.rend(); ++entry) {
        auto filesystem = *entry;
        auto is_main = (filesystem == list.front());
        auto result = std::invoke(callback, filesystem, is_main);

        if (result) {
          return result;
        }
      }
    }

    return result_type{};
  }

  filesystem_map _filesystems;
  std::vector<alias> _sorted_alias;

  mutable std::mutex _mutex;

}; // class virtual_filesystem

using virtual_filesystem_ptr = std::shared_ptr<virtual_filesystem>;
using virtual_filesystem_weak_ptr = std::weak_ptr<virtual_filesystem>;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_VIRTUAL_FILESYSTEM_HPP_
