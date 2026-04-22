// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_MEMORY_FILE_HPP_
#define LIBSBX_FILESYSTEM_MEMORY_FILE_HPP_

#include <cstring>
#include <mutex>
#include <algorithm>

#include <libsbx/utility/lockable.hpp>

#include <libsbx/filesystem/file_base.hpp>
#include <libsbx/filesystem/file_info.hpp>

namespace sbx::filesystem {

namespace detail {

template<utility::lockable Lockable>
class memory_file_object {

public:

  using lockable_type = Lockable;

  memory_file_object()
  : _data{std::make_shared<std::vector<std::uint8_t>>()} {}

  memory_file_object(const memory_file_object& other) {
    _copy_from(other);
  }

  auto operator=(const memory_file_object& other) -> memory_file_object& {
    if (this != &other) {
      _copy_from(other);
    }

    return *this;
  }

  [[nodiscard]] auto data() const -> std::shared_ptr<std::vector<std::uint8_t>> {
    auto lock = std::scoped_lock{_mutex};

    return _data;
  }

  [[nodiscard]] auto writable_data() -> std::shared_ptr<std::vector<std::uint8_t>> {
    auto lock = std::scoped_lock{_mutex};

    if (!_data || _data.use_count() == 1) {
      return _data;
    }

    _data = std::make_shared<std::vector<std::uint8_t>>(*_data);

    return _data;
  }

  auto reset() -> void {
    auto lock = std::scoped_lock{_mutex};

    _data = std::make_shared<std::vector<std::uint8_t>>();
  }

private:

  auto _copy_from(const memory_file_object& other) -> void {
    auto other_data = other.data();

    auto lock = std::scoped_lock{_mutex};

    if (!other_data) {
      _data = std::make_shared<std::vector<std::uint8_t>>();
    } else {
      _data = std::make_shared<std::vector<std::uint8_t>>(*other_data);
    }
  }

private:

  mutable lockable_type _mutex;
  std::shared_ptr<std::vector<std::uint8_t>> _data;

}; // memory_file_object

template<utility::lockable Lockable>
using memory_file_object_ptr = std::shared_ptr<memory_file_object<Lockable>>;

} // namespace detail

template<utility::lockable Lockable>
class basic_memory_file final : public file_base {

public:

  using lockable_type = Lockable;

  basic_memory_file(file_info&& info, detail::memory_file_object_ptr<lockable_type>&& object)
  : _info(std::move(info)),
    _object(std::move(object)) {
    if (!_object) {
      _object = std::make_shared<detail::memory_file_object<lockable_type>>();
    }
  }

  ~basic_memory_file() override {
    _is_open = false;
    _position = 0;
    _mode = mode::read;
  }

  [[nodiscard]] auto info() const -> const file_info& override {
    auto lock = std::scoped_lock{_mutex};

    return _info;
  }

  [[nodiscard]] auto size() const -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_is_open) {
      return 0;
    }

    auto data = _object->data();

    return data ? data->size() : 0;
  }

  [[nodiscard]] auto is_read_only() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    return !mode_has_flag(_mode, mode::write);
  }

  [[nodiscard]] auto open(const mode mode) -> bool override {
    auto lock = std::scoped_lock{_mutex};

    if (!is_mode_valid(mode)) {
      return false;
    }

    if (_is_open && _mode == mode) {
      _position = 0;

      return true;
    }

    _mode = mode;
    _position = 0;

    if (mode_has_flag(mode, mode::append)) {
      auto data = _object->data();

      _position = data ? data->size() : 0;
    }

    if (mode_has_flag(mode, mode::truncate)) {
      _object->reset();
    }

    _is_open = true;

    return true;
  }

  auto close() -> void override {
    auto lock = std::scoped_lock{_mutex};

    _is_open = false;
    _position = 0;
    _mode = mode::read;
  }

  [[nodiscard]] auto is_open() const -> bool override {
    auto lock = std::scoped_lock{_mutex};

    return _is_open;
  }

  auto seek(std::uint64_t offset, origin origin) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_is_open) {
      return 0;
    }

    auto data = _object->data();

    const auto size = data ? data->size() : 0;

    switch (origin) {
      case origin::begin: {
        _position = offset;
        break;
      }
      case origin::end: {
        _position = (offset < size) ? size - offset : 0;
        break;
      }
      case origin::set: {
        _position += offset;
        break;
      }
    }

    _position = std::min(_position, size);

    return _position;
  }

  [[nodiscard]] auto tell() const -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    return _position;
  }

  auto read(std::span<std::uint8_t> buffer) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_is_open || !mode_has_flag(_mode, mode::read)) {
      return 0;
    }

    auto data = _object->data();

    if (!data) {
      return 0;
    }

    if (_position >= data->size()) {
      return 0;
    }

    const auto to_read = std::min<std::uint64_t>(buffer.size(), data->size() - _position);

    if (to_read == 0) {
      return 0;
    }

    std::memcpy(buffer.data(), data->data() + _position, to_read);
    _position += to_read;

    return to_read;
  }

  auto read(std::vector<std::uint8_t>& buffer, std::uint64_t size) -> std::uint64_t override {
    buffer.resize(size);

    return read(std::span<std::uint8_t>(buffer));
  }

  auto write(std::span<const std::uint8_t> buffer) -> std::uint64_t override {
    auto lock = std::scoped_lock{_mutex};

    if (!_is_open || !mode_has_flag(_mode, mode::write)) {
      return 0;
    }

    auto data = _object->writable_data();

    if (!data) {
      return 0;
    }

    const auto write_size = buffer.size();

    if (write_size == 0) {
      return 0;
    }

    if (_position + write_size > data->size()) {
      data->resize(_position + write_size);
    }

    std::memcpy(data->data() + _position, buffer.data(), write_size);

    _position += write_size;

    return write_size;
  }

  auto write(const std::vector<std::uint8_t>& buffer) -> std::uint64_t override {
    return write(std::span<const std::uint8_t>(buffer));
  }

private:

  mutable lockable_type _mutex;

  file_info _info;
  detail::memory_file_object_ptr<lockable_type> _object;

  bool _is_open{false};
  std::uint64_t _position{0};
  mode _mode{mode::read};

}; // class basic_memory_file

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_MEMORY_FILE_HPP_
