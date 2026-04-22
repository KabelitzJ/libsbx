// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_NATIVE_FILESYSTEM_HPP_
#define LIBSBX_FILESYSTEM_NATIVE_FILESYSTEM_HPP_

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <algorithm>

#include <libsbx/utility/lockable.hpp>

#include <libsbx/filesystem/filesystem_base.hpp>
#include <libsbx/filesystem/native_file.hpp>

namespace sbx::filesystem {

template<utility::lockable Lockable>
class basic_native_filesystem final : public filesystem_base {

public:

  using lockable_type = Lockable;

  basic_native_filesystem(const std::string& alias_path, const std::string& base_path)
  : _alias_path{alias_path}, 
    _base_path{base_path} { }

  ~basic_native_filesystem() override {
    shutdown();
  }

  auto initialize() -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (_is_initialized) {
      return true;
    }

    if (!std::filesystem::exists(_base_path) || !std::filesystem::is_directory(_base_path)) {
      return false;
    }

    _build_file_list(_alias_path, _base_path);
    _is_initialized = true;

    return true;
  }

  auto shutdown() -> void override {
    auto lock = std::scoped_lock{_mutex};

    _base_path.clear();
    _alias_path.clear();
    _files.clear();
    _is_initialized = false;
  }

  [[nodiscard]] auto is_initialized() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    return _is_initialized;
  }

  [[nodiscard]] auto base_path() const -> const std::string& override {
    auto lock = std::scoped_lock{_mutex};

    return _base_path;
  }

  [[nodiscard]] auto virtual_path() const -> const std::string& override {
    auto lock = std::scoped_lock{_mutex};

    return _alias_path;
  }

  [[nodiscard]] auto is_read_only() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (!_is_initialized) {
      return true;
    }

    auto permissions = std::filesystem::status(_base_path).permissions();
    return (permissions & std::filesystem::perms::owner_write) == std::filesystem::perms::none;
  }

  auto open_file(const std::string& path, file_base::mode mode) -> std::shared_ptr<file_base> override {
    auto lock = std::scoped_lock{_mutex};

    const auto request_write = file_base::mode_has_flag(mode, file_base::mode::write);

    if (request_write) {
      auto permissions = std::filesystem::status(_base_path).permissions();

      if ((permissions & std::filesystem::perms::owner_write) == std::filesystem::perms::none) {
        return nullptr;
      }
    }

    auto entry = _files.find(path);

    if (entry == _files.end() && request_write) {
      auto info = file_info{_alias_path, _base_path, path};
      entry = _files.emplace(path, file_entry{info}).first;
    }

    if (entry == _files.end()) {
      return nullptr;
    }

    auto& file_entry = entry->second;
    auto file = std::make_shared<basic_native_file<lockable_type>>(file_entry.info);

    if (!file || !file->open(mode)) {
      return nullptr;
    }

    file_entry.opened_handles.push_back(file);

    return file;
  }

  auto close_file(const file_ptr& file) -> void override {
    auto lock = std::scoped_lock{_mutex};

    if (file) {
      file->close();
    }

    for (auto& [path, entry] : _files) {
      entry.cleanup_opened_handles(file);
    }
  }

  auto remove_file(const std::string& path) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (is_read_only()) {
      return false;
    }

    const auto entry = _files.find(path);

    if (entry == _files.end()) {
      return false;
    }

    const auto native_path = entry->second.info.native_path();
    
    _files.erase(entry);

    return std::filesystem::remove(native_path);
  }

  [[nodiscard]] auto exists(const std::string& path) const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    const auto entry = _files.find(path);

    return entry != _files.end() && std::filesystem::exists(entry->second.info.native_path());
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

  [[nodiscard]] auto create_file(const std::string& path) -> file_ptr override {
    return open_file(path, file_base::mode::write | file_base::mode::truncate);
  }

  auto copy_file(const std::string& source, const std::string& destination, const bool overwrite = false) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (is_read_only()) {
      return false;
    }

    const auto entry = _files.find(source);

    if (entry == _files.end()) {
      return false;
    }

    const auto& source_info = entry->second.info;
    const auto destination_info = file_info{_alias_path, _base_path, destination};

    auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::skip_existing;
    
    auto ec = std::error_code{};

    if (!std::filesystem::copy_file(source_info.native_path(), destination_info.native_path(), options, ec)) {
      return false;
    }

    _files.insert_or_assign(destination, file_entry{destination_info});

    return true;
  }

  auto rename_file(const std::string& source, const std::string& destination) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (is_read_only()) {
      return false;
    }

    const auto entry = _files.find(source);

    if (entry == _files.end()) {
      return false;
    }

    const auto& source_info = entry->second.info;
    const auto destination_info = file_info{_alias_path, _base_path, destination};

    auto ec = std::error_code{};
    std::filesystem::rename(source_info.native_path(), destination_info.native_path(), ec);
    
    if (ec) {
      return false;
    }

    _files.erase(entry);
    _files.insert_or_assign(destination, file_entry{destination_info});

    return true;
  }

private:

  struct file_entry {

    file_info info;
    std::vector<std::weak_ptr<basic_native_file<Lockable>>> opened_handles;

    explicit file_entry(const file_info& info)
    : info{info} { }

    auto cleanup_opened_handles(const std::shared_ptr<file_base>& file_to_exclude = nullptr) -> void {
      opened_handles.erase(std::remove_if(opened_handles.begin(), opened_handles.end(), [&](const auto& weak) {
        return weak.expired() || weak.lock() == file_to_exclude;
      }), opened_handles.end());
    }

  }; // struct file_entry

  auto _build_file_list(const std::string& alias, const std::string& current_path) -> void {
    for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
      if (entry.is_directory()) {
        _build_file_list(alias, entry.path().string());
        continue;
      }

      auto info = file_info{alias, _base_path, entry.path().string()};
      _files.emplace(info.virtual_path(), file_entry{info});
    }
  }

  mutable lockable_type _mutex;

  std::string _alias_path;
  std::string _base_path;
  bool _is_initialized{false};

  std::unordered_map<std::string, file_entry> _files;

}; // class basic_native_filesystem

using native_filesystem_mt = basic_native_filesystem<std::mutex>;
using native_filesystem_st = basic_native_filesystem<utility::null_mutex>;

using native_filesystem = native_filesystem_mt;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_NATIVE_FILESYSTEM_HPP_
