// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/collision_cache_io.hpp>

#include <system_error>

#include <fmt/format.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

namespace sbx::physics {

struct cache_header {
  std::uint32_t magic{0};
  std::uint32_t format_version{0};
  std::uint64_t source_hash{0};
}; // struct cache_header

auto collision_cache_path(std::string_view extension, const math::uuid& mesh_id) -> std::filesystem::path {
  return core::engine::project().library_directory() / fmt::format("{}{}", mesh_id.value(), extension);
}

auto open_collision_cache_for_read(const std::filesystem::path& path, std::uint32_t expected_magic, std::uint32_t expected_format_version, std::uint64_t expected_source_hash) -> std::optional<std::ifstream> {
  auto stream = std::ifstream{path, std::ios::binary};

  if (!stream.is_open()) {
    return std::nullopt;
  }

  auto header = cache_header{};
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!stream || header.magic != expected_magic || header.format_version != expected_format_version || header.source_hash != expected_source_hash) {
    return std::nullopt;
  }

  return stream;
}

auto open_collision_cache_for_write(const std::filesystem::path& path, std::uint32_t magic, std::uint32_t format_version, std::uint64_t source_hash) -> std::optional<std::ofstream> {
  auto error = std::error_code{};
  std::filesystem::create_directories(path.parent_path(), error);

  auto stream = std::ofstream{path, std::ios::binary | std::ios::trunc};

  if (!stream.is_open()) {
    return std::nullopt;
  }

  const auto header = cache_header{magic, format_version, source_hash};
  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));

  if (!stream) {
    return std::nullopt;
  }

  return stream;
}

} // namespace sbx::physics
