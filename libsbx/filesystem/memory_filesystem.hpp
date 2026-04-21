// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_
#define LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <algorithm>

#include <libsbx/filesystem/filesystem_base.hpp>
#include <libsbx/filesystem/memory_file.hpp>

namespace sbx::filesystem {

class memory_filesystem final : public filesystem_base {

public:

  explicit memory_filesystem(std::string alias_path)
  : _alias_path(std::move(alias_path)) {}

  ~memory_filesystem() override {
    shutdown();
  }

  [[nodiscard]] auto initialize() -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (_initialized) {
      return true;
    }

    _initialized = true;
    return true;
  }

  auto shutdown() -> void override {
    auto lock = std::scoped_lock{_mutex};

    _files.clear();
    _initialized = false;
  }

  [[nodiscard]] auto is_initialized() const -> bool override {
    auto lock = std::scoped_lock{_mutex};
    return _initialized;
  }

  [[nodiscard]] auto base_path() const -> const std::string& override {
    auto lock = std::scoped_lock{_mutex};
    return _alias_path;
  }

  [[nodiscard]] auto virtual_path() const -> const std::string& override {
    auto lock = std::scoped_lock{_mutex};
    return _alias_path;
  }

  [[nodiscard]] auto files() const -> files_list override {
    auto lock = std::scoped_lock{_mutex};

    files_list list;
    list.reserve(_files.size());

    for (const auto& [path, entry] : _files) {
      list.push_back(entry.info);
    }

    return list;
  }

  [[nodiscard]] auto is_read_only() const -> bool override {
    return false;
  }

  [[nodiscard]] auto open_file(const std::string& path, const file_base::mode mode) -> file_ptr override {
    auto lock = std::scoped_lock{_mutex};

    auto info = file_info{_alias_path, _alias_path, path};

    auto [it, inserted] = _files.try_emplace(
      path,
      file_entry{info, std::make_shared<memory_file_object>()}
    );

    auto& entry = it->second;

    if (!entry.object) {
      entry.object = std::make_shared<memory_file_object>();
    }

    auto file = std::make_shared<memory_file>(
      file_info(entry.info),
      memory_file_object_ptr(entry.object)
    );

    if (!file || !file->open(mode)) {
      return nullptr;
    }

    entry.opened_handles.push_back(file);
    return file;
  }

  auto close_file(file_ptr file) -> void override {
    auto lock = std::scoped_lock{_mutex};
    _cleanup_handles(file);
  }

  [[nodiscard]] auto create_file(const std::string& path) -> file_ptr override {
    return open_file(path, file::mode::read_write | file::mode::truncate);
  }

  [[nodiscard]] auto remove_file(const std::string& path) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    auto it = _files.find(path);
    if (it == _files.end()) {
      return false;
    }

    _cleanup_handles();
    _files.erase(it);

    return true;
  }

  [[nodiscard]] auto copy_file(const std::string& src,
                               const std::string& dst,
                               const bool overwrite = false) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    auto src_it = _files.find(src);
    if (src_it == _files.end()) {
      return false;
    }

    auto dst_it = _files.find(dst);

    if (dst_it != _files.end() && !overwrite) {
      return false;
    }

    if (dst_it != _files.end()) {
      _files.erase(dst_it);
    }

    memory_file_object_ptr new_object;

    if (src_it->second.object) {
      new_object = std::make_shared<memory_file_object>(
        *src_it->second.object
      );
    } else {
      new_object = std::make_shared<memory_file_object>();
    }

    file_info info(_alias_path, _alias_path, dst);

    _files.emplace(dst, file_entry{info, std::move(new_object)});
    return true;
  }

  [[nodiscard]] auto rename_file(const std::string& src,
                                 const std::string& dst) -> bool override {
    if (!copy_file(src, dst, false)) {
      return false;
    }

    return remove_file(src);
  }

  [[nodiscard]] auto exists(const std::string& path) const -> bool override {
    auto lock = std::scoped_lock{_mutex};
    return _files.find(path) != _files.end();
  }

private:

  struct file_entry {

    file_info info;
    memory_file_object_ptr object;

    using weak_handle = std::weak_ptr<memory_file>;
    std::vector<weak_handle> opened_handles;

    void cleanup(file_ptr exclude = nullptr) {
      opened_handles.erase(
        std::remove_if(
          opened_handles.begin(),
          opened_handles.end(),
          [&](const weak_handle& w) {
            return w.expired() || w.lock() == exclude;
          }
        ),
        opened_handles.end()
      );
    }
  };

  auto _cleanup_handles(file_ptr to_close = nullptr) -> void {

    if (to_close) {
      const auto& path = to_close->file_info().virtual_path();

      auto it = _files.find(path);
      if (it != _files.end()) {
        to_close->close();
      }
    }

    for (auto& [_, entry] : _files) {
      entry.cleanup(to_close);
    }
  }

private:

  std::string _alias_path;
  bool _initialized{false};

  mutable std::mutex _mutex;
  std::unordered_map<std::string, file_entry> _files;
};

using memory_filesystem_ptr = std::shared_ptr<memory_filesystem>;
using memory_filesystem_weak_ptr = std::weak_ptr<memory_filesystem>;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_
