// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/collision_cache_io.hpp
 *
 * @brief Shared plumbing for disk-persisting mesh_collision_cache's and convex_hull_cache's
 * derived data across process restarts -- mirrors the engine's existing cache idioms (asset_cooker,
 * shader_disk_cache): a FourCC magic + format-version + content-hash header, raw struct
 * ofstream::write/ifstream::read (no serialization library), any mismatch on read treated as a
 * silent cache miss. Purely file I/O -- neither mesh_collision_cache nor convex_hull_cache's actual
 * data types (a dynamic_tree, a point/face set) know anything about files; that stays their
 * respective _build()'s job, using the helpers here.
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
 * @brief Path for one mesh's derived-collision-data cache file: `project().library_directory() /
 * "{mesh_id}{extension}"` -- the same {uuid}+extension convention asset_cooker already uses for its
 * own cooked-asset blobs (.sbxmsh, .sbxtex, ...). A direct 1:1 mapping; no separate lookup table.
 */
[[nodiscard]] auto collision_cache_path(std::string_view extension, const math::uuid& mesh_id) -> std::filesystem::path;

/**
 * @brief Opens `path` for reading and validates its header: magic, format_version, and a caller-
 * supplied content hash (of whatever source data the cache actually depends on) must all match
 * exactly. Returns the stream, positioned right after the header, on a match; nullopt on any miss --
 * a missing file, wrong magic/version, a mismatched hash, and a truncated read are all just "not
 * usable", the caller never needs to know which.
 */
[[nodiscard]] auto open_collision_cache_for_read(const std::filesystem::path& path, std::uint32_t expected_magic, std::uint32_t expected_format_version, std::uint64_t expected_source_hash) -> std::optional<std::ifstream>;

/**
 * @brief Opens `path` for writing (creating parent directories as needed) and writes the header;
 * the caller appends its own payload immediately after. Returns nullopt only if the file couldn't be
 * opened for writing -- a failure here just means this run doesn't get a warm cache next time, never
 * a hard error worth propagating.
 */
[[nodiscard]] auto open_collision_cache_for_write(const std::filesystem::path& path, std::uint32_t magic, std::uint32_t format_version, std::uint64_t source_hash) -> std::optional<std::ofstream>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLISION_CACHE_IO_HPP_
