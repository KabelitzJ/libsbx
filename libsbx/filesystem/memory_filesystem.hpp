// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_
#define LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <algorithm>

#include <libsbx/utility/lockable.hpp>

#include <libsbx/filesystem/filesystem_base.hpp>
#include <libsbx/filesystem/memory_file.hpp>

namespace sbx::filesystem {

template<utility::lockable Lockable>
class basic_memory_filesystem final : public filesystem_base {

public:

  using lockable_type = Lockable;

  explicit basic_memory_filesystem(std::string alias_path)
  : _alias_path(std::move(alias_path)) {}

  ~basic_memory_filesystem() override {
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

    auto list = files_list{};
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

    auto [entry, inserted] = _files.try_emplace(path, file_entry{info, std::make_shared<detail::memory_file_object<lockable_type>>()});

    auto& file_entry = entry->second;

    if (!file_entry.object) {
      file_entry.object = std::make_shared<detail::memory_file_object<lockable_type>>();
    }

    auto file = std::make_shared<basic_memory_file<lockable_type>>(file_info{file_entry.info}, detail::memory_file_object_ptr<lockable_type>{file_entry.object});

    if (!file || !file->open(mode)) {
      return nullptr;
    }

    file_entry.opened_handles.push_back(file);

    return file;
  }

  auto close_file(const file_ptr& file) -> void override {
    auto lock = std::scoped_lock{_mutex};

    _cleanup_handles(file);
  }

  [[nodiscard]] auto create_file(const std::string& path) -> file_ptr override {
    return open_file(path, file_base::mode::read_write | file_base::mode::truncate);
  }

  [[nodiscard]] auto remove_file(const std::string& path) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    auto entry = _files.find(path);

    if (entry == _files.end()) {
      return false;
    }

    _cleanup_handles();
    _files.erase(entry);

    return true;
  }

  [[nodiscard]] auto copy_file(const std::string& source, const std::string& destination, const bool overwrite = false) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    auto source_entry = _files.find(source);

    if (source_entry == _files.end()) {
      return false;
    }

    auto destination_entry = _files.find(destination);

    if (destination_entry != _files.end() && !overwrite) {
      return false;
    }

    if (destination_entry != _files.end()) {
      _files.erase(destination_entry);
    }

    auto new_object = detail::memory_file_object_ptr<lockable_type>{};

    if (source_entry->second.object) {
      new_object = std::make_shared<detail::memory_file_object<lockable_type>>(*source_entry->second.object);
    } else {
      new_object = std::make_shared<detail::memory_file_object<lockable_type>>();
    }

    auto info = file_info{_alias_path, _alias_path, destination};

    _files.emplace(destination, file_entry{info, std::move(new_object)});

    return true;
  }

  [[nodiscard]] auto rename_file(const std::string& source, const std::string& destination) -> bool override {
    if (!copy_file(source, destination, false)) {
      return false;
    }

    return remove_file(source);
  }

  [[nodiscard]] auto exists(const std::string& path) const -> bool override {
    auto lock = std::scoped_lock{_mutex};
    return _files.find(path) != _files.end();
  }

private:

  struct file_entry {

    using weak_handle = std::weak_ptr<basic_memory_file<lockable_type>>;

    file_info info;
    detail::memory_file_object_ptr<lockable_type> object;
    std::vector<weak_handle> opened_handles;

    void cleanup(const file_ptr& exclude = nullptr) {
      opened_handles.erase(std::remove_if(opened_handles.begin(), opened_handles.end(), [&](const weak_handle& handle) {
        return handle.expired() || handle.lock() == exclude;
      }), opened_handles.end());
    }

  }; // struct file_entry

  auto _cleanup_handles(const file_ptr& to_close = nullptr) -> void {
    if (to_close) {
      const auto& path = to_close->info().virtual_path();

      auto entry = _files.find(path);

      if (entry != _files.end()) {
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

  mutable lockable_type _mutex;
  std::unordered_map<std::string, file_entry> _files;

}; // class basic_memory_filesystem

using memory_filesystem_mt = basic_memory_filesystem<std::mutex>;
using memory_filesystem_st = basic_memory_filesystem<utility::null_mutex>;

using memory_filesystem = memory_filesystem_mt;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_MEMORY_FILESYSTEM_HPP_
