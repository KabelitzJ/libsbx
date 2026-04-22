// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_NATIVE_FILE_HPP_
#define LIBSBX_FILESYSTEM_NATIVE_FILE_HPP_

#include <cstdio>
#include <mutex>
#include <vector>
#include <span>
#include <string>
#include <system_error>
#include <algorithm>

#include <libsbx/utility/lockable.hpp>

#include <libsbx/filesystem/file_base.hpp>
#include <libsbx/filesystem/file_info.hpp>

namespace sbx::filesystem {

template<typename Lockable>
class basic_native_file final : public file_base {

public:

  using handle_type = std::FILE*;
  using lockable_type = Lockable;

  explicit basic_native_file(const file_info& info)
  : _info{info} { }

  basic_native_file(const file_info& info, handle_type stream)
  : _info{info}, 
    _file{stream} { }

  ~basic_native_file() override {
    close();
  }

  [[nodiscard]] auto info() const -> const file_info& override {
    auto lock = std::scoped_lock{_mutex};

    return _info;
  }

  [[nodiscard]] auto size() const -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_file) {
      return 0;
    }

    auto ec = std::error_code{};
    auto size = std::filesystem::file_size(_info.native_path(), ec);

    return ec ? 0 : static_cast<std::uint64_t>(size);
  }

  [[nodiscard]] auto is_read_only() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    return !file_base::mode_has_flag(_mode, mode::write);
  }

  [[nodiscard]]
  auto open(mode mode) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (!file_base::is_mode_valid(mode)) {
      return false;
    }

    if (_file && _mode == mode) {
      std::fseek(_file, 0, SEEK_SET);
      return true;
    }

    _mode = mode;

    const auto is_read = file_base::mode_has_flag(mode, mode::read);
    const auto is_write = file_base::mode_has_flag(mode, mode::write);
    const auto is_append = file_base::mode_has_flag(mode, mode::append);
    const auto is_truncate = file_base::mode_has_flag(mode, mode::truncate);

    auto mode_string = std::string{};

    if (is_read && !is_write) {
      mode_string = "rb";
    } else if (is_write && !is_read) {
      mode_string = is_append ? "ab" : (is_truncate ? "wb" : "r+b");
    } else if (is_read && is_write) {
      mode_string = is_append ? "a+b" : (is_truncate ? "w+b" : "r+b");
    } else {
      mode_string = "rb";
    }

    _file = std::fopen(_info.native_path().c_str(), mode_string.c_str());

    return _file != nullptr;
  }

  auto close() -> void override {
    auto lock = std::scoped_lock{_mutex};

    if (_file) {
      std::fclose(_file);
      _file = nullptr;
      _mode = mode::read;
    }
  }

  [[nodiscard]] auto is_open() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    return _file != nullptr;
  }

  auto seek(std::uint64_t offset, origin origin) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_file) {
      return 0;
    }

    auto whence = SEEK_SET;

    switch (origin) {
      case origin::begin: {
        whence = SEEK_SET; 
        break;
      }
      case origin::end: {
        whence = SEEK_END; 
        break;
      }
      case origin::set: { 
        whence = SEEK_CUR; 
        break;
      }
    }

    if (std::fseek(_file, static_cast<long>(offset), whence) != 0) {
      return 0;
    }

    const auto position = std::ftell(_file);

    return (position != -1L) ? static_cast<std::uint64_t>(position) : 0;
  }

  [[nodiscard]] auto tell() const -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_file) {
      return 0;
    }

    const auto position = std::ftell(_file);

    return (position != -1L) ? static_cast<std::uint64_t>(position) : 0;
  }

  auto read(std::span<std::uint8_t> buffer) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_file || !file_base::mode_has_flag(_mode, mode::read)) {
      return 0;
    }

    const auto read_count = std::fread(buffer.data(), 1, buffer.size(), _file);

    return static_cast<std::uint64_t>(read_count);
  }

  auto read(std::vector<std::uint8_t>& buffer, std::uint64_t size) -> std::uint64_t override {
    buffer.resize(size);
    return read(std::span<std::uint8_t>{buffer});
  }

  auto write(std::span<const std::uint8_t> buffer) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_file || !file_base::mode_has_flag(_mode, mode::write)) {
      return 0;
    }

    if (buffer.empty()) {
      return 0;
    }

    const auto written = std::fwrite(buffer.data(), 1, buffer.size(), _file);

    return static_cast<std::uint64_t>(written);
  }

  auto write(const std::vector<std::uint8_t>& buffer) -> std::uint64_t override {
    return write(std::span<const std::uint8_t>{buffer});
  }

private:

  mutable lockable_type _mutex;

  file_info _info;
  handle_type _file = nullptr;
  mode _mode = mode::read;

}; // class basic_native_file

using native_file_mt = basic_native_file<std::mutex>;
using native_file_st = basic_native_file<utility::null_mutex>;

using native_file = native_file_mt;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_NATIVE_FILE_HPP_
