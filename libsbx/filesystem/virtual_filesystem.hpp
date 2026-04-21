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

#include <libsbx/filesystem/filesystem.hpp>
#include <libsbx/filesystem/file_base.hpp>
#include <libsbx/filesystem/alias.hpp>

namespace sbx::filesystem {

class virtual_filesystem final {

public:

  using filesystem_list = std::vector<filesystem_ptr>;
  using filesystem_map  = std::unordered_map<alias, filesystem_list, alias::hash>;

  virtual_filesystem() = default;

  ~virtual_filesystem() {
    std::scoped_lock lock{_mutex};

    for (auto& [_, list] : _filesystems) {
      for (auto& fs : list) {
        if (fs) {
          fs->shutdown();
        }
      }
    }
  }

  void add_filesystem(const alias& a, filesystem_ptr fs) {
    if (!fs) return;

    std::scoped_lock lock{_mutex};

    _filesystems[a].push_back(fs);

    if (std::find(_sorted_alias.begin(), _sorted_alias.end(), a) == _sorted_alias.end()) {
      _sorted_alias.push_back(a);
    }

    std::sort(_sorted_alias.begin(), _sorted_alias.end(),
      [](const alias& lhs, const alias& rhs) {
        return lhs.length() > rhs.length();
      });
  }

  void add_filesystem(std::string a, filesystem_ptr fs) {
    add_filesystem(alias(std::move(a)), fs);
  }

  template <typename FilesystemT, typename... Args>
  [[nodiscard]]
  auto create_filesystem(const alias& a, Args&&... args)
    -> std::optional<std::shared_ptr<FilesystemT>>
  {
    auto fs = std::make_shared<FilesystemT>(a.string(), std::forward<Args>(args)...);

    if (!fs || !fs->initialize()) {
      return std::nullopt;
    }

    add_filesystem(a, fs);
    return fs;
  }

  template <typename FilesystemT, typename... Args>
  [[nodiscard]]
  auto create_filesystem(std::string a, Args&&... args)
    -> std::optional<std::shared_ptr<FilesystemT>>
  {
    return create_filesystem<FilesystemT>(alias(std::move(a)), std::forward<Args>(args)...);
  }

  void remove_filesystem(const alias& a, filesystem_ptr fs) {
    std::scoped_lock lock{_mutex};

    auto it = _filesystems.find(a);
    if (it == _filesystems.end()) return;

    auto& list = it->second;

    list.erase(std::remove(list.begin(), list.end(), fs), list.end());

    if (list.empty()) {
      _filesystems.erase(it);
      _sorted_alias.erase(
        std::remove(_sorted_alias.begin(), _sorted_alias.end(), a),
        _sorted_alias.end()
      );
    }
  }

  void remove_filesystem(std::string a, filesystem_ptr fs) {
    remove_filesystem(alias(std::move(a)), fs);
  }

  [[nodiscard]]
  auto has_filesystem(const alias& a, filesystem_ptr fs) const -> bool {
    std::scoped_lock lock{_mutex};

    auto it = _filesystems.find(a);
    if (it == _filesystems.end()) return false;

    const auto& list = it->second;
    return std::find(list.begin(), list.end(), fs) != list.end();
  }

  [[nodiscard]]
  auto has_filesystem(std::string a, filesystem_ptr fs) const -> bool {
    return has_filesystem(alias(std::move(a)), fs);
  }

  void unregister_alias(const alias& a) {
    std::scoped_lock lock{_mutex};

    _filesystems.erase(a);
    _sorted_alias.erase(
      std::remove(_sorted_alias.begin(), _sorted_alias.end(), a),
      _sorted_alias.end()
    );
  }

  void unregister_alias(std::string a) {
    unregister_alias(alias(std::move(a)));
  }

  [[nodiscard]]
  auto is_alias_registered(const alias& a) const -> bool {
    std::scoped_lock lock{_mutex};
    return _filesystems.find(a) != _filesystems.end();
  }

  [[nodiscard]]
  auto is_alias_registered(std::string a) const -> bool {
    return is_alias_registered(alias(std::move(a)));
  }

  [[nodiscard]]
  auto filesystems(const alias& a)
    -> std::optional<std::reference_wrapper<const filesystem_list>>
  {
    std::scoped_lock lock{_mutex};

    auto it = _filesystems.find(a);
    if (it != _filesystems.end()) {
      return std::cref(it->second);
    }

    return std::nullopt;
  }

  [[nodiscard]]
  auto filesystems(std::string a)
    -> std::optional<std::reference_wrapper<const filesystem_list>>
  {
    return filesystems(alias(std::move(a)));
  }

private:

  template <typename Callback>
  auto _visit(const std::string& path, Callback&& cb) const {
    using result_t = decltype(cb(std::declval<filesystem_ptr>(), bool{}));

    for (const auto& a : _sorted_alias) {
      if (!path.starts_with(a.string())) continue;

      auto it = _filesystems.find(a);
      if (it == _filesystems.end()) continue;

      const auto& list = it->second;

      for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
        auto fs = *rit;
        bool is_main = (fs == list.front());

        auto result = cb(fs, is_main);
        if (result) {
          return result;
        }
      }
    }

    return result_t{};
  }

public:

  [[nodiscard]]
  auto open_file(const std::string& path, file_base::mode mode) -> file_ptr {
    std::scoped_lock lock{_mutex};

    auto result = _visit(path,
      [&](filesystem_ptr fs, bool) -> std::optional<file_ptr> {

        if (fs->exists(path)) {
          if (auto f = fs->open_file(path, mode)) {
            return f;
          }
        }
        else if (!fs->is_read_only() &&
                 file_base::mode_has_flag(mode, file_base::mode::write)) {
          if (auto f = fs->open_file(path, mode)) {
            return f;
          }
        }

        return std::nullopt;
      });

    return result.value_or(nullptr);
  }

  [[nodiscard]]
  auto exists(const std::string& path) const -> bool {
    std::scoped_lock lock{_mutex};

    auto result = _visit(path,
      [&](filesystem_ptr fs, bool) -> std::optional<bool> {
        if (fs->exists(path)) {
          return true;
        }
        return std::nullopt;
      });

    return result.value_or(false);
  }

  [[nodiscard]]
  auto list_all_files() const -> std::vector<std::string> {
    std::scoped_lock lock{_mutex};

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const auto& a : _sorted_alias) {

      auto it = _filesystems.find(a);
      if (it == _filesystems.end()) continue;

      const auto& list = it->second;

      for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {

        auto fs = *rit;
        auto files = fs->files();

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

private:

  filesystem_map _filesystems;
  std::vector<alias> _sorted_alias;

  mutable std::mutex _mutex;
};

using virtual_filesystem_ptr = std::shared_ptr<virtual_filesystem>;
using virtual_filesystem_weak_ptr = std::weak_ptr<virtual_filesystem>;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_VIRTUAL_FILESYSTEM_HPP_
