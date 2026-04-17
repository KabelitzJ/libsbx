// SPDX-License-Identifier: MIT
#include <libsbx/io/read_file.hpp>

#include <fstream>

#include <fmt/format.h>

namespace sbx::io {


auto read_file(const std::filesystem::path& path) -> std::vector<std::uint8_t> {
  auto file = std::ifstream{path, std::ios::ate | std::ios::binary};
  auto content = std::vector<std::uint8_t>{};

  if (!file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open file: {}", path.string())};
  }

  const auto size = static_cast<std::size_t>(file.tellg());

  file.seekg(std::ios::beg);

  content.resize(size);

  file.read(reinterpret_cast<char*>(content.data()), static_cast<std::streamsize>(size));

  file.close();

  return content;
}

auto load_spirv_words(const std::filesystem::path& p) -> std::vector<std::uint32_t> {
  std::ifstream f(p, std::ios::binary);
  if (!f) throw std::runtime_error("Cannot open file: " + p.string());

  f.seekg(0, std::ios::end);
  const auto n = static_cast<std::size_t>(f.tellg());
  f.seekg(0, std::ios::beg);

  if (n == 0) throw std::runtime_error("File is empty: " + p.string());
  if (n % 4 != 0) throw std::runtime_error("SPIR-V size not multiple of 4: " + p.string());

  std::vector<std::uint32_t> words(n / 4);
  f.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(n));
  if (!f) throw std::runtime_error("Failed to read file: " + p.string());

  constexpr std::uint32_t SpvMagic = 0x07230203u;
  if (words[0] != SpvMagic)
    throw std::runtime_error("Bad SPIR-V magic (not a .spv?): " + p.string());

  return words;
}

} // namespace sbx::io
