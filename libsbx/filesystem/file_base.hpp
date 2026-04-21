// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_FILE_BASE_HPP_
#define LIBSBX_FILESYSTEM_FILE_BASE_HPP_

#include <span>
#include <vector>
#include <memory>

#include <libsbx/utility/enum.hpp>

#include <libsbx/filesystem/file_info.hpp>

namespace sbx::filesystem {

class file_base {

public:

  enum class origin {
    begin,
    end,
    set
  }; // enum class origin
    
  enum class mode : std::uint8_t {
    read = utility::bit_v<0>,
    write = utility::bit_v<1>,
    append = utility::bit_v<2>,
    truncate = utility::bit_v<3>,
    read_write = read | write
  }; // enum class mode


  file_base() = default;

  virtual ~file_base() = default;
  
  [[nodiscard]] virtual auto file_info() const -> const file_info& = 0;
  
  [[nodiscard]] virtual auto size() const -> std::uint64_t = 0;

  [[nodiscard]] virtual auto is_read_only() const -> bool = 0;
  
  [[nodiscard]] virtual auto open(mode mode) -> bool = 0;
  
  virtual auto close() -> void = 0;
  
  [[nodiscard]] virtual auto is_open() const -> bool = 0;
  
  virtual auto seek(std::uint64_t offset, origin origin) -> std::uint64_t = 0;

  [[nodiscard]] virtual auto tell() const -> std::uint64_t = 0;

  virtual auto read(std::span<uint8_t> buffer) -> std::uint64_t = 0;

  virtual auto read(std::vector<std::uint8_t>& buffer, std::uint64_t size) -> std::uint64_t = 0;

  virtual auto write(std::span<const std::uint8_t> buffer) -> std::uint64_t = 0;
  
  virtual auto write(const std::vector<std::uint8_t>& buffer) -> std::uint64_t = 0;

  static auto mode_has_flag(mode file_mode, mode flag) -> bool {
    return (static_cast<std::uint8_t>(file_mode) & static_cast<std::uint8_t>(flag)) != 0;
  }

  [[nodiscard]] static auto is_mode_valid(mode file_mode) -> bool {
    if (!mode_has_flag(file_mode, file_mode::read) && !mode_has_flag(file_mode, file_mode::write)) {
      return false;
    }

    if (mode_has_flag(file_mode, file_mode::append) && !mode_has_flag(file_mode, file_mode::write)) {
      return false;
    }

    if (mode_has_flag(file_mode, file_mode::truncate) && !mode_has_flag(file_mode, file_mode::write)) {
      return false;
    }

    return true;
  }

}; // class file

using file_ptr = std::shared_ptr<file_base>;
using file_weak_ptr = std::weak_ptr<file_base>;

inline bool operator==(const file_ptr& lhs, const file_ptr& rhs) {
  if (!lhs || !rhs) {
    return false;
  }
  
  return lhs->file_info() == rhs->file_info();
}

} // namespace sbx::filesystem

template<>
struct sbx::utility::is_bit_field<sbx::filesystem::file::mode> : std::true_type { };

#endif // LIBSBX_FILESYSTEM_FILE_BASE_HPP_
