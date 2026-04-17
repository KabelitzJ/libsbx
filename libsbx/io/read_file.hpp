// SPDX-License-Identifier: MIT
#ifndef LIBSBX_IO_READ_FILE_HPP_
#define LIBSBX_IO_READ_FILE_HPP_

#include <filesystem>
#include <vector>

namespace sbx::io {

auto read_file(const std::filesystem::path& path) -> std::vector<std::uint8_t>;

auto load_spirv_words(const std::filesystem::path& p) -> std::vector<std::uint32_t>;

} // namespace sbx::io

#endif // LIBSBX_IO_READ_FILE_HPP_
