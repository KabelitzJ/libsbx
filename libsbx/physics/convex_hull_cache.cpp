// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/convex_hull_cache.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/physics/collision_cache_io.hpp>
#include <libsbx/physics/quickhull.hpp>

namespace sbx::physics {

constexpr auto cache_magic = utility::fourcc_v<"SBCH">;
constexpr auto cache_format_version = std::uint32_t{1};
constexpr auto cache_extension = std::string_view{".sbxhull"};

// The hull itself is the expensive result, so points and faces are both persisted.
[[nodiscard]] auto compute_source_hash(const std::vector<math::vector3>& positions) -> std::uint64_t {
  auto bytes = std::vector<std::uint8_t>{};
  bytes.resize(positions.size() * sizeof(math::vector3));

  std::memcpy(bytes.data(), positions.data(), bytes.size());

  return utility::djb2_hash<std::uint64_t>{}(bytes);
}

[[nodiscard]] auto try_read_disk_cache(const math::uuid& mesh_id, std::uint64_t source_hash) -> std::optional<convex_hull_data> {
  auto stream = open_collision_cache_for_read(collision_cache_path(cache_extension, mesh_id), cache_magic, cache_format_version, source_hash);

  if (!stream) {
    return std::nullopt;
  }

  auto data = convex_hull_data{};

  auto point_count = std::uint32_t{0};
  stream->read(reinterpret_cast<char*>(&point_count), sizeof(point_count));

  if (!*stream || point_count > convex_hull_max_points) {
    return std::nullopt;
  }

  for (auto index = std::uint32_t{0}; index < point_count; ++index) {
    auto point = math::vector3{};
    stream->read(reinterpret_cast<char*>(&point), sizeof(point));
    data.points.push_back(point);
  }

  auto face_count = std::uint32_t{0};
  stream->read(reinterpret_cast<char*>(&face_count), sizeof(face_count));

  if (!*stream || face_count > convex_hull_max_faces) {
    return std::nullopt;
  }

  for (auto index = std::uint32_t{0}; index < face_count; ++index) {
    auto face = convex_hull_face{};
    stream->read(reinterpret_cast<char*>(&face), sizeof(face));
    data.faces.push_back(face);
  }

  auto bounds_min = math::vector3{};
  auto bounds_max = math::vector3{};
  stream->read(reinterpret_cast<char*>(&bounds_min), sizeof(bounds_min));
  stream->read(reinterpret_cast<char*>(&bounds_max), sizeof(bounds_max));

  if (!*stream) {
    return std::nullopt;
  }

  data.local_bounds = math::volume{bounds_min, bounds_max};

  return data;
}

auto write_disk_cache(const math::uuid& mesh_id, const convex_hull_data& data, std::uint64_t source_hash) -> void {
  auto stream = open_collision_cache_for_write(collision_cache_path(cache_extension, mesh_id), cache_magic, cache_format_version, source_hash);

  if (!stream) {
    return;
  }

  const auto point_count = static_cast<std::uint32_t>(data.points.size());
  stream->write(reinterpret_cast<const char*>(&point_count), sizeof(point_count));

  for (const auto& point : data.points) {
    stream->write(reinterpret_cast<const char*>(&point), sizeof(point));
  }

  const auto face_count = static_cast<std::uint32_t>(data.faces.size());
  stream->write(reinterpret_cast<const char*>(&face_count), sizeof(face_count));

  for (const auto& face : data.faces) {
    stream->write(reinterpret_cast<const char*>(&face), sizeof(face));
  }

  const auto bounds_min = data.local_bounds.min();
  const auto bounds_max = data.local_bounds.max();
  stream->write(reinterpret_cast<const char*>(&bounds_min), sizeof(bounds_min));
  stream->write(reinterpret_cast<const char*>(&bounds_max), sizeof(bounds_max));
}

// Farthest-point sampling: start from the 6 axis extremes, then repeatedly add the point farthest from those picked. Only the fallback for hulls over the cap.
[[nodiscard]] auto farthest_point_sample(const std::vector<math::vector3>& points, std::size_t max_count) -> std::vector<math::vector3> {
  if (points.size() <= max_count) {
    return points;
  }

  auto selected = std::vector<std::size_t>{};
  selected.reserve(max_count);

  const auto try_add = [&](std::size_t index) {
    if (selected.size() >= max_count || std::ranges::find(selected, index) != selected.end()) {
      return;
    }

    selected.push_back(index);
  };

  for (auto axis = std::size_t{0}; axis < 3u; ++axis) {
    auto min_index = std::size_t{0};
    auto max_index = std::size_t{0};

    for (auto index = std::size_t{1}; index < points.size(); ++index) {
      if (points[index][axis] < points[min_index][axis]) {
        min_index = index;
      }

      if (points[index][axis] > points[max_index][axis]) {
        max_index = index;
      }
    }

    try_add(min_index);
    try_add(max_index);
  }

  while (selected.size() < max_count && selected.size() < points.size()) {
    auto best_index = std::optional<std::size_t>{};
    auto best_min_distance_squared = -1.0f;

    for (auto index = std::size_t{0}; index < points.size(); ++index) {
      if (std::ranges::find(selected, index) != selected.end()) {
        continue;
      }

      auto min_distance_squared = std::numeric_limits<std::float_t>::max();

      for (const auto selected_index : selected) {
        min_distance_squared = std::min(min_distance_squared, math::vector3::distance_squared(points[index], points[selected_index]));
      }

      if (min_distance_squared > best_min_distance_squared) {
        best_min_distance_squared = min_distance_squared;
        best_index = index;
      }
    }

    if (!best_index) {
      break;
    }

    selected.push_back(*best_index);
  }

  auto result = std::vector<math::vector3>{};
  result.reserve(selected.size());

  for (const auto index : selected) {
    result.push_back(points[index]);
  }

  return result;
}

auto convex_hull_cache::get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const convex_hull_data& {
  if (const auto entry = _cache.find(mesh_id); entry != _cache.end()) {
    return *entry->second;
  }

  return *_cache.emplace(mesh_id, std::make_unique<const convex_hull_data>(_build(assets_module, mesh_id))).first->second;
}

auto convex_hull_cache::clear() -> void {
  _cache.clear();
}

auto convex_hull_cache::_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> convex_hull_data {
  auto data = convex_hull_data{};

  const auto cooked = assets_module.resolve_mesh_collision_data(mesh_id);

  if (!cooked || cooked->vertices.empty()) {
    return data;
  }

  auto positions = std::vector<math::vector3>{};
  positions.reserve(cooked->vertices.size());

  for (const auto& vertex : cooked->vertices) {
    positions.push_back(vertex.position);
  }

  const auto source_hash = compute_source_hash(positions);

  if (auto cached = try_read_disk_cache(mesh_id, source_hash)) {
    return std::move(*cached);
  }

  auto hull = compute_convex_hull(positions);

  if (hull.vertices.size() > convex_hull_max_points) {
    hull = compute_convex_hull(farthest_point_sample(hull.vertices, convex_hull_max_points));
  }

  for (const auto& vertex : hull.vertices) {
    data.points.push_back(vertex);
  }

  for (const auto& face : hull.faces) {
    data.faces.push_back(convex_hull_face{{
      static_cast<std::uint16_t>(face.indices[0]),
      static_cast<std::uint16_t>(face.indices[1]),
      static_cast<std::uint16_t>(face.indices[2])
    }});
  }

  data.local_bounds = math::volume::construct(data.points);

  write_disk_cache(mesh_id, data, source_hash);

  return data;
}

} // namespace sbx::physics
