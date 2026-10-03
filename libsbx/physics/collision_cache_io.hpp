// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/collision_cache_io.hpp
 *
 * @brief Disk persistence plumbing for the collision caches: a FourCC, format version and content hash header with raw struct I/O; any mismatch is a silent miss.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_COLLISION_CACHE_IO_HPP_
#define LIBSBX_PHYSICS_COLLISION_CACHE_IO_HPP_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include <libsbx/math/uuid.hpp>

namespace sbx::physics {

/**
 * @brief A mesh's collision cache file: `library_directory() / "{mesh_id}{extension}"`, like cooked assets.
 *
 * @param extension The cache file extension.
 * @param mesh_id The mesh's uuid.
 *
 * @return The path.
 */
[[nodiscard]] auto collision_cache_path(std::string_view extension, const math::uuid& mesh_id) -> std::filesystem::path;

/**
 * @brief Opens a cache file and validates its header.
 *
 * @param path The cache file.
 * @param expected_magic The FourCC magic.
 * @param expected_format_version The format version.
 * @param expected_source_hash The hash of the source data the cache depends on.
 *
 * @return The stream positioned after the header, or nullopt for any miss (missing, mismatched or truncated).
 */
[[nodiscard]] auto open_collision_cache_for_read(const std::filesystem::path& path, std::uint32_t expected_magic, std::uint32_t expected_format_version, std::uint64_t expected_source_hash) -> std::optional<std::ifstream>;

/**
 * @brief Creates a cache file (and its directories) and writes the header; the caller appends the payload.
 *
 * @param path The cache file.
 * @param magic The FourCC magic.
 * @param format_version The format version.
 * @param source_hash The hash of the source data.
 *
 * @return The stream, or nullopt if it couldn't be opened, which just means no warm cache next run.
 */
[[nodiscard]] auto open_collision_cache_for_write(const std::filesystem::path& path, std::uint32_t magic, std::uint32_t format_version, std::uint64_t source_hash) -> std::optional<std::ofstream>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLISION_CACHE_IO_HPP_
