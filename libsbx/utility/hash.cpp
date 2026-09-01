// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/utility/hash.hpp>

namespace sbx::utility {

auto hash_file(const std::filesystem::path& path) -> std::uint64_t {
  auto in = std::ifstream{path, std::ios::binary};

  if (!in) {
    return 0u;
  }

  using hasher_type = fnv1a_hash<char, std::uint64_t>;

  auto hasher = hasher_type{};

  auto hash = hasher_type::basis;
  auto buffer = std::array<char, 65536u>{};

  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = static_cast<std::size_t>(in.gcount());

    hasher(hash, {buffer.data(), count});
  }

  return hash;
}

} // namespace sbx::utility
