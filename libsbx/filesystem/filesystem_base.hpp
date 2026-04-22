// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_FILESYSTEM_BASE_HPP_
#define LIBSBX_FILESYSTEM_FILESYSTEM_BASE_HPP_

#include <memory>
#include <string>
#include <vector>

#include <libsbx/filesystem/file_base.hpp>
#include <libsbx/filesystem/file_info.hpp>

namespace sbx::filesystem {

class filesystem_base {

public:

  using files_list = std::vector<file_info>;

  filesystem_base() = default;

  virtual ~filesystem_base() = default;

  [[nodiscard]] virtual auto initialize() -> bool = 0;

  virtual auto shutdown() -> void = 0;

  [[nodiscard]] virtual auto is_initialized() const -> bool = 0;

  [[nodiscard]] virtual auto base_path() const -> const std::string& = 0;

  [[nodiscard]] virtual auto virtual_path() const -> const std::string& = 0;

  [[nodiscard]] virtual auto files() const -> files_list = 0;

  [[nodiscard]] virtual auto is_read_only() const -> bool = 0;

  [[nodiscard]] virtual auto open_file(const std::string& path, const file_base::mode mode) -> file_ptr = 0;

  virtual auto close_file(const file_ptr& file) -> void = 0;

  [[nodiscard]] virtual auto create_file(const std::string& path) -> file_ptr = 0;

  [[nodiscard]] virtual auto remove_file(const std::string& path) -> bool = 0;

  [[nodiscard]] virtual auto copy_file(const std::string& source, const std::string& destination, const bool overwrite = false) -> bool = 0;

  [[nodiscard]] virtual auto rename_file(const std::string& source, const std::string& destination) -> bool = 0;

  [[nodiscard]] virtual auto exists(const std::string& path) const -> bool = 0;

}; // class filesystem_base

using filesystem_ptr = std::shared_ptr<filesystem_base>;
using filesystem_weak_ptr = std::weak_ptr<filesystem_base>;

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_FILESYSTEM_BASE_HPP_
