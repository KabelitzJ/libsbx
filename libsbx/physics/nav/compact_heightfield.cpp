// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/compact_heightfield.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace sbx::physics {

[[nodiscard]] auto get_span_count(const heightfield& hf) -> std::int32_t {
  auto count = std::int32_t{0};

  for (const auto& column : hf.columns) {
    for (auto current = column; current; current = current->next) {
      if (current->area != null_area) {
        ++count;
      }
    }
  }

  return count;
}

[[nodiscard]] auto build_compact_heightfield(std::int32_t walkable_height, std::int32_t walkable_climb, const heightfield& hf, bake_arena& arena) -> compact_heightfield {
  auto chf = compact_heightfield{arena.permanent()};

  const auto span_count = get_span_count(hf);

  chf.width = hf.width;
  chf.height = hf.height;
  chf.walkable_height = walkable_height;
  chf.walkable_climb = walkable_climb;
  chf.bounds = math::volume{hf.bounds.min(), hf.bounds.max() + math::vector3{0.0f, static_cast<std::float_t>(walkable_height) * hf.cell_height, 0.0f}};
  chf.cell_size = hf.cell_size;
  chf.cell_height = hf.cell_height;

  chf.cells.resize(static_cast<std::size_t>(hf.width) * static_cast<std::size_t>(hf.height));
  chf.spans.resize(static_cast<std::size_t>(span_count));
  chf.areas.assign(static_cast<std::size_t>(span_count), null_area);

  auto current_cell_index = std::int32_t{0};
  const auto num_columns = hf.width * hf.height;

  for (auto column_index = std::int32_t{0}; column_index < num_columns; ++column_index) {
    auto current = hf.columns[static_cast<std::size_t>(column_index)];

    if (!current) {
      continue;
    }

    auto& cell = chf.cells[static_cast<std::size_t>(column_index)];
    cell.index = static_cast<std::uint32_t>(current_cell_index);
    cell.count = 0u;

    for (; current; current = current->next) {
      if (current->area != null_area) {
        const auto bot = static_cast<std::int32_t>(current->max);
        const auto top = current->next ? static_cast<std::int32_t>(current->next->min) : 0xffff;

        auto& span = chf.spans[static_cast<std::size_t>(current_cell_index)];
        span.min = static_cast<std::uint16_t>(std::clamp(bot, 0, 0xffff));
        span.height = static_cast<std::uint32_t>(std::clamp(top - bot, 0, 0xff));

        chf.areas[static_cast<std::size_t>(current_cell_index)] = static_cast<std::uint8_t>(current->area);

        ++current_cell_index;
        ++cell.count;
      }
    }
  }

  const auto max_layers = static_cast<std::int32_t>(not_connected) - 1;

  for (auto z = std::int32_t{0}; z < hf.height; ++z) {
    for (auto x = std::int32_t{0}; x < hf.width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * hf.width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        auto& span = chf.spans[static_cast<std::size_t>(i)];

        for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
          set_connection(span, direction, static_cast<std::int32_t>(not_connected));

          const auto neighbor_x = x + direction_offset_x(direction);
          const auto neighbor_z = z + direction_offset_z(direction);

          if (neighbor_x < 0 || neighbor_z < 0 || neighbor_x >= hf.width || neighbor_z >= hf.height) {
            continue;
          }

          const auto& neighbor_cell = chf.cells[static_cast<std::size_t>(neighbor_x + neighbor_z * hf.width)];

          for (auto k = static_cast<std::int32_t>(neighbor_cell.index), nk = static_cast<std::int32_t>(neighbor_cell.index + neighbor_cell.count); k < nk; ++k) {
            const auto& neighbor_span = chf.spans[static_cast<std::size_t>(k)];

            const auto bot = std::max(static_cast<std::int32_t>(span.min), static_cast<std::int32_t>(neighbor_span.min));
            const auto top = std::min(static_cast<std::int32_t>(span.min) + static_cast<std::int32_t>(span.height), static_cast<std::int32_t>(neighbor_span.min) + static_cast<std::int32_t>(neighbor_span.height));

            if ((top - bot) >= walkable_height && std::abs(static_cast<std::int32_t>(neighbor_span.min) - static_cast<std::int32_t>(span.min)) <= walkable_climb) {
              const auto layer_index = k - static_cast<std::int32_t>(neighbor_cell.index);

              if (layer_index >= 0 && layer_index <= max_layers) {
                set_connection(span, direction, layer_index);
              }

              break;
            }
          }
        }
      }
    }
  }

  return chf;
}

auto erode_walkable_area(std::int32_t erosion_radius, compact_heightfield& chf, bake_arena& arena) -> void {
  const auto width = chf.width;
  const auto height = chf.height;
  const auto span_count = static_cast<std::int32_t>(chf.spans.size());

  auto distance_to_boundary = std::pmr::vector<std::uint8_t>(static_cast<std::size_t>(span_count), std::uint8_t{0xff}, arena.temp());

  for (auto z = std::int32_t{0}; z < height; ++z) {
    for (auto x = std::int32_t{0}; x < width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        if (chf.areas[static_cast<std::size_t>(i)] == null_area) {
          distance_to_boundary[static_cast<std::size_t>(i)] = 0;
          continue;
        }

        const auto& span = chf.spans[static_cast<std::size_t>(i)];

        auto neighbor_count = std::int32_t{0};

        for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
          const auto neighbor_connection = get_connection(span, direction);

          if (neighbor_connection == static_cast<std::int32_t>(not_connected)) {
            break;
          }

          const auto neighbor_x = x + direction_offset_x(direction);
          const auto neighbor_z = z + direction_offset_z(direction);
          const auto neighbor_span_index = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(neighbor_x + neighbor_z * width)].index) + neighbor_connection;

          if (chf.areas[static_cast<std::size_t>(neighbor_span_index)] == null_area) {
            break;
          }

          ++neighbor_count;
        }

        if (neighbor_count != 4) {
          distance_to_boundary[static_cast<std::size_t>(i)] = 0;
        }
      }
    }
  }

  const auto step = [&](std::int32_t i, const compact_span& span, std::int32_t x, std::int32_t z, std::int32_t primary_direction, std::int32_t secondary_direction) -> void {
    if (get_connection(span, primary_direction) == static_cast<std::int32_t>(not_connected)) {
      return;
    }

    const auto ax = x + direction_offset_x(primary_direction);
    const auto az = z + direction_offset_z(primary_direction);
    const auto a_index = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(ax + az * width)].index) + get_connection(span, primary_direction);
    const auto& a_span = chf.spans[static_cast<std::size_t>(a_index)];

    auto new_distance = static_cast<std::uint8_t>(std::min(static_cast<std::int32_t>(distance_to_boundary[static_cast<std::size_t>(a_index)]) + 2, 255));

    if (new_distance < distance_to_boundary[static_cast<std::size_t>(i)]) {
      distance_to_boundary[static_cast<std::size_t>(i)] = new_distance;
    }

    if (get_connection(a_span, secondary_direction) == static_cast<std::int32_t>(not_connected)) {
      return;
    }

    const auto bx = ax + direction_offset_x(secondary_direction);
    const auto bz = az + direction_offset_z(secondary_direction);
    const auto b_index = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(bx + bz * width)].index) + get_connection(a_span, secondary_direction);

    new_distance = static_cast<std::uint8_t>(std::min(static_cast<std::int32_t>(distance_to_boundary[static_cast<std::size_t>(b_index)]) + 3, 255));

    if (new_distance < distance_to_boundary[static_cast<std::size_t>(i)]) {
      distance_to_boundary[static_cast<std::size_t>(i)] = new_distance;
    }
  };

  for (auto z = std::int32_t{0}; z < height; ++z) {
    for (auto x = std::int32_t{0}; x < width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];
      const auto max_span_index = static_cast<std::int32_t>(cell.index + cell.count);

      for (auto i = static_cast<std::int32_t>(cell.index); i < max_span_index; ++i) {
        const auto& span = chf.spans[static_cast<std::size_t>(i)];
        step(i, span, x, z, 0, 3);
        step(i, span, x, z, 3, 2);
      }
    }
  }

  for (auto z = height - 1; z >= 0; --z) {
    for (auto x = width - 1; x >= 0; --x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];
      const auto max_span_index = static_cast<std::int32_t>(cell.index + cell.count);

      for (auto i = static_cast<std::int32_t>(cell.index); i < max_span_index; ++i) {
        const auto& span = chf.spans[static_cast<std::size_t>(i)];
        step(i, span, x, z, 2, 1);
        step(i, span, x, z, 1, 0);
      }
    }
  }

  const auto min_boundary_distance = static_cast<std::uint8_t>(erosion_radius * 2);

  for (auto i = std::int32_t{0}; i < span_count; ++i) {
    if (distance_to_boundary[static_cast<std::size_t>(i)] < min_boundary_distance) {
      chf.areas[static_cast<std::size_t>(i)] = null_area;
    }
  }
}

auto paint_rect_region(std::int32_t min_x, std::int32_t max_x, std::int32_t min_z, std::int32_t max_z, std::uint16_t region_id, compact_heightfield& chf, std::pmr::vector<std::uint16_t>& src_regions) -> void {
  const auto width = chf.width;

  for (auto z = min_z; z < max_z; ++z) {
    for (auto x = min_x; x < max_x; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        if (chf.areas[static_cast<std::size_t>(i)] != null_area) {
          src_regions[static_cast<std::size_t>(i)] = region_id;
        }
      }
    }
  }
}

struct sweep_span {
  std::uint16_t region_id{0};
  std::uint16_t id{0};
  std::uint16_t sample_count{0};
  std::uint16_t neighbor_id{0};
}; // struct sweep_span

auto add_unique_neighbor(std::pmr::vector<std::uint16_t>& neighbors, std::uint16_t region_id) -> void {
  if (std::find(neighbors.begin(), neighbors.end(), region_id) == neighbors.end()) {
    neighbors.push_back(region_id);
  }
}

auto merge_and_filter_regions(compact_heightfield& chf, std::int32_t min_region_area, std::int32_t merge_region_area, std::pmr::vector<std::uint16_t>& src_regions, std::uint16_t region_count, bake_arena& arena) -> void {
  const auto width = chf.width;
  const auto height = chf.height;

  auto region_span_counts = std::pmr::vector<std::int32_t>(static_cast<std::size_t>(region_count), 0, arena.temp());

  for (const auto value : src_regions) {
    if (value != 0) {
      ++region_span_counts[value];
    }
  }

  for (auto id = std::uint16_t{1}; id < region_count; ++id) {
    if (region_span_counts[static_cast<std::size_t>(id)] < min_region_area) {
      region_span_counts[static_cast<std::size_t>(id)] = 0;
    }
  }

  for (auto& value : src_regions) {
    if (value != 0 && region_span_counts[static_cast<std::size_t>(value)] == 0) {
      value = 0;
    }
  }

  auto region_neighbors = std::pmr::vector<std::pmr::vector<std::uint16_t>>(static_cast<std::size_t>(region_count), arena.temp());

  for (auto z = std::int32_t{0}; z < height; ++z) {
    for (auto x = std::int32_t{0}; x < width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        const auto region = src_regions[static_cast<std::size_t>(i)];

        if (region == 0) {
          continue;
        }

        const auto& span = chf.spans[static_cast<std::size_t>(i)];

        for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
          const auto connection = get_connection(span, direction);

          if (connection == static_cast<std::int32_t>(not_connected)) {
            continue;
          }

          const auto neighbor_x = x + direction_offset_x(direction);
          const auto neighbor_z = z + direction_offset_z(direction);
          const auto neighbor_index = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(neighbor_x + neighbor_z * width)].index) + connection;
          const auto neighbor_region = src_regions[static_cast<std::size_t>(neighbor_index)];

          if (neighbor_region == 0 || neighbor_region == region) {
            continue;
          }

          add_unique_neighbor(region_neighbors[region], neighbor_region);
          add_unique_neighbor(region_neighbors[neighbor_region], region);
        }
      }
    }
  }

  auto remap = std::pmr::vector<std::uint16_t>(static_cast<std::size_t>(region_count), arena.temp());

  for (auto id = std::uint16_t{0}; id < region_count; ++id) {
    remap[id] = id;
  }

  auto merged_any = true;

  while (merged_any) {
    merged_any = false;

    for (auto id = std::uint16_t{1}; id < region_count; ++id) {
      if (region_span_counts[static_cast<std::size_t>(id)] == 0 || region_span_counts[static_cast<std::size_t>(id)] > merge_region_area) {
        continue;
      }

      auto best_neighbor = std::uint16_t{0};
      auto best_count = std::numeric_limits<std::int32_t>::max();

      for (const auto neighbor_id : region_neighbors[id]) {
        const auto neighbor_count = region_span_counts[static_cast<std::size_t>(neighbor_id)];

        if (neighbor_count > 0 && neighbor_count < best_count) {
          best_count = neighbor_count;
          best_neighbor = neighbor_id;
        }
      }

      if (best_neighbor == 0) {
        continue;
      }

      region_span_counts[static_cast<std::size_t>(best_neighbor)] += region_span_counts[static_cast<std::size_t>(id)];
      region_span_counts[static_cast<std::size_t>(id)] = 0;

      for (const auto other : region_neighbors[id]) {
        if (other != best_neighbor) {
          add_unique_neighbor(region_neighbors[best_neighbor], other);
          add_unique_neighbor(region_neighbors[other], best_neighbor);
        }
      }

      for (auto& target : remap) {
        if (target == id) {
          target = best_neighbor;
        }
      }

      merged_any = true;
    }
  }

  for (auto& value : src_regions) {
    if (value != 0 && (value & border_region) == 0) {
      value = remap[value];
    }
  }
}

[[nodiscard]] auto build_regions_monotone(compact_heightfield& chf, std::int32_t border_size, std::int32_t min_region_area, std::int32_t merge_region_area, bake_arena& arena) -> bool {
  constexpr auto null_neighbor = std::uint16_t{0xffff};

  const auto width = chf.width;
  const auto height = chf.height;

  auto region_id = std::uint16_t{1};

  auto src_regions = std::pmr::vector<std::uint16_t>(chf.spans.size(), std::uint16_t{0}, arena.temp());

  const auto sweep_count = static_cast<std::size_t>(std::max(width, height)) + 1u;
  auto sweeps = std::pmr::vector<sweep_span>(sweep_count, arena.temp());

  if (border_size > 0) {
    const auto bw = std::min(width, border_size);
    const auto bh = std::min(height, border_size);

    paint_rect_region(0, bw, 0, height, static_cast<std::uint16_t>(region_id | border_region), chf, src_regions); ++region_id;
    paint_rect_region(width - bw, width, 0, height, static_cast<std::uint16_t>(region_id | border_region), chf, src_regions); ++region_id;
    paint_rect_region(0, width, 0, bh, static_cast<std::uint16_t>(region_id | border_region), chf, src_regions); ++region_id;
    paint_rect_region(0, width, height - bh, height, static_cast<std::uint16_t>(region_id | border_region), chf, src_regions); ++region_id;
  }

  chf.border_size = border_size;

  auto previous_counts = std::pmr::vector<std::int32_t>{arena.temp()};

  for (auto z = border_size; z < height - border_size; ++z) {
    previous_counts.assign(static_cast<std::size_t>(region_id) + 1u, 0);

    auto row_id = std::uint16_t{1};

    for (auto x = border_size; x < width - border_size; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        const auto& span = chf.spans[static_cast<std::size_t>(i)];

        if (chf.areas[static_cast<std::size_t>(i)] == null_area) {
          continue;
        }

        auto previous_id = std::uint16_t{0};

        if (get_connection(span, 0) != static_cast<std::int32_t>(not_connected)) {
          const auto ax = x + direction_offset_x(0);
          const auto az = z + direction_offset_z(0);
          const auto ai = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(ax + az * width)].index) + get_connection(span, 0);

          if ((src_regions[static_cast<std::size_t>(ai)] & border_region) == 0 && chf.areas[static_cast<std::size_t>(i)] == chf.areas[static_cast<std::size_t>(ai)]) {
            previous_id = src_regions[static_cast<std::size_t>(ai)];
          }
        }

        if (!previous_id) {
          previous_id = row_id++;
          sweeps[previous_id].region_id = previous_id;
          sweeps[previous_id].sample_count = 0;
          sweeps[previous_id].neighbor_id = 0;
        }

        if (get_connection(span, 3) != static_cast<std::int32_t>(not_connected)) {
          const auto ax = x + direction_offset_x(3);
          const auto az = z + direction_offset_z(3);
          const auto ai = static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(ax + az * width)].index) + get_connection(span, 3);

          if (src_regions[static_cast<std::size_t>(ai)] != 0 && (src_regions[static_cast<std::size_t>(ai)] & border_region) == 0 && chf.areas[static_cast<std::size_t>(i)] == chf.areas[static_cast<std::size_t>(ai)]) {
            const auto neighbor_region = src_regions[static_cast<std::size_t>(ai)];

            if (sweeps[previous_id].neighbor_id == 0 || sweeps[previous_id].neighbor_id == neighbor_region) {
              sweeps[previous_id].neighbor_id = neighbor_region;
              ++sweeps[previous_id].sample_count;
              ++previous_counts[neighbor_region];
            } else {
              sweeps[previous_id].neighbor_id = null_neighbor;
            }
          }
        }

        src_regions[static_cast<std::size_t>(i)] = previous_id;
      }
    }

    for (auto i = std::uint16_t{1}; i < row_id; ++i) {
      if (sweeps[i].neighbor_id != null_neighbor && sweeps[i].neighbor_id != 0 && previous_counts[sweeps[i].neighbor_id] == static_cast<std::int32_t>(sweeps[i].sample_count)) {
        sweeps[i].id = sweeps[i].neighbor_id;
      } else {
        sweeps[i].id = region_id++;
      }
    }

    for (auto x = border_size; x < width - border_size; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        if (src_regions[static_cast<std::size_t>(i)] > 0 && src_regions[static_cast<std::size_t>(i)] < row_id) {
          src_regions[static_cast<std::size_t>(i)] = sweeps[src_regions[static_cast<std::size_t>(i)]].id;
        }
      }
    }
  }

  chf.max_regions = region_id;

  merge_and_filter_regions(chf, min_region_area, merge_region_area, src_regions, region_id, arena);

  for (auto i = std::size_t{0}; i < chf.spans.size(); ++i) {
    chf.spans[i].region_id = src_regions[i];
  }

  return true;
}

struct level_entry {
  std::int32_t x;
  std::int32_t z;
  std::int32_t index;
}; // struct level_entry

using level_stack = std::pmr::vector<level_entry>;

[[nodiscard]] auto neighbor_span_index(const compact_heightfield& chf, std::int32_t x, std::int32_t z, const compact_span& span, std::int32_t direction) -> std::int32_t {
  const auto neighbor_x = x + direction_offset_x(direction);
  const auto neighbor_z = z + direction_offset_z(direction);

  return static_cast<std::int32_t>(chf.cells[static_cast<std::size_t>(neighbor_x + neighbor_z * chf.width)].index) + get_connection(span, direction);
}

/** Chamfer distance (2 straight, 3 diagonal) from every span to the nearest unwalkable or area boundary. Returns the maximum. */
[[nodiscard]] auto calculate_distance_field(const compact_heightfield& chf, std::pmr::vector<std::uint16_t>& distance) -> std::uint16_t {
  const auto width = chf.width;
  const auto height = chf.height;

  std::fill(distance.begin(), distance.end(), std::uint16_t{0xffff});

  for (auto z = std::int32_t{0}; z < height; ++z) {
    for (auto x = std::int32_t{0}; x < width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        const auto& span = chf.spans[static_cast<std::size_t>(i)];
        const auto area = chf.areas[static_cast<std::size_t>(i)];

        auto neighbor_count = std::int32_t{0};

        for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
          if (get_connection(span, direction) != static_cast<std::int32_t>(not_connected) && chf.areas[static_cast<std::size_t>(neighbor_span_index(chf, x, z, span, direction))] == area) {
            ++neighbor_count;
          }
        }

        if (neighbor_count != 4) {
          distance[static_cast<std::size_t>(i)] = 0;
        }
      }
    }
  }

  const auto step = [&](std::int32_t i, const compact_span& span, std::int32_t x, std::int32_t z, std::int32_t primary_direction, std::int32_t secondary_direction) -> void {
    if (get_connection(span, primary_direction) == static_cast<std::int32_t>(not_connected)) {
      return;
    }

    const auto a_index = neighbor_span_index(chf, x, z, span, primary_direction);
    const auto& a_span = chf.spans[static_cast<std::size_t>(a_index)];

    distance[static_cast<std::size_t>(i)] = std::min(distance[static_cast<std::size_t>(i)], static_cast<std::uint16_t>(std::min(distance[static_cast<std::size_t>(a_index)] + 2, 0xffff)));

    if (get_connection(a_span, secondary_direction) == static_cast<std::int32_t>(not_connected)) {
      return;
    }

    const auto b_index = neighbor_span_index(chf, x + direction_offset_x(primary_direction), z + direction_offset_z(primary_direction), a_span, secondary_direction);

    distance[static_cast<std::size_t>(i)] = std::min(distance[static_cast<std::size_t>(i)], static_cast<std::uint16_t>(std::min(distance[static_cast<std::size_t>(b_index)] + 3, 0xffff)));
  };

  for (auto z = std::int32_t{0}; z < height; ++z) {
    for (auto x = std::int32_t{0}; x < width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        step(i, chf.spans[static_cast<std::size_t>(i)], x, z, 0, 3);
        step(i, chf.spans[static_cast<std::size_t>(i)], x, z, 3, 2);
      }
    }
  }

  for (auto z = height - 1; z >= 0; --z) {
    for (auto x = width - 1; x >= 0; --x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        step(i, chf.spans[static_cast<std::size_t>(i)], x, z, 2, 1);
        step(i, chf.spans[static_cast<std::size_t>(i)], x, z, 1, 0);
      }
    }
  }

  return distance.empty() ? std::uint16_t{0} : *std::max_element(distance.begin(), distance.end());
}

/** 3x3 box blur of the distance field, skipping spans within @p threshold of a boundary so thin corridors keep their shape. */
auto blur_distance_field(const compact_heightfield& chf, std::int32_t threshold, const std::pmr::vector<std::uint16_t>& source, std::pmr::vector<std::uint16_t>& destination) -> void {
  threshold *= 2;

  for (auto z = std::int32_t{0}; z < chf.height; ++z) {
    for (auto x = std::int32_t{0}; x < chf.width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * chf.width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        const auto& span = chf.spans[static_cast<std::size_t>(i)];
        const auto own = static_cast<std::int32_t>(source[static_cast<std::size_t>(i)]);

        if (own <= threshold) {
          destination[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(own);
          continue;
        }

        auto sum = own;

        for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
          if (get_connection(span, direction) == static_cast<std::int32_t>(not_connected)) {
            sum += own * 2;
            continue;
          }

          const auto a_index = neighbor_span_index(chf, x, z, span, direction);
          const auto& a_span = chf.spans[static_cast<std::size_t>(a_index)];
          const auto next_direction = (direction + 1) & 0x3;

          sum += source[static_cast<std::size_t>(a_index)];

          if (get_connection(a_span, next_direction) != static_cast<std::int32_t>(not_connected)) {
            sum += source[static_cast<std::size_t>(neighbor_span_index(chf, x + direction_offset_x(direction), z + direction_offset_z(direction), a_span, next_direction))];
          } else {
            sum += own;
          }
        }

        destination[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>((sum + 5) / 9);
      }
    }
  }
}

/** Grows a new region from one seed span over spans at least two levels below @p level, stopping where it would touch another region. */
[[nodiscard]] auto flood_region(std::int32_t x, std::int32_t z, std::int32_t i, std::uint16_t level, std::uint16_t region, const compact_heightfield& chf, const std::pmr::vector<std::uint16_t>& distance, std::pmr::vector<std::uint16_t>& src_regions, std::pmr::vector<std::uint16_t>& src_distance, level_stack& stack) -> bool {
  const auto area = chf.areas[static_cast<std::size_t>(i)];
  const auto lowest = static_cast<std::uint16_t>(level >= 2 ? level - 2 : 0);

  stack.clear();
  stack.push_back(level_entry{x, z, i});
  src_regions[static_cast<std::size_t>(i)] = region;
  src_distance[static_cast<std::size_t>(i)] = 0;

  auto count = std::int32_t{0};

  while (!stack.empty()) {
    const auto current = stack.back();
    stack.pop_back();

    const auto& span = chf.spans[static_cast<std::size_t>(current.index)];

    // A span next to a different region stays unassigned, so neighboring floods don't merge.
    auto touches_other = false;

    for (auto direction = std::int32_t{0}; direction < 4 && !touches_other; ++direction) {
      if (get_connection(span, direction) == static_cast<std::int32_t>(not_connected)) {
        continue;
      }

      const auto a_index = neighbor_span_index(chf, current.x, current.z, span, direction);

      if (chf.areas[static_cast<std::size_t>(a_index)] != area) {
        continue;
      }

      const auto a_region = src_regions[static_cast<std::size_t>(a_index)];

      if (a_region & border_region) {
        continue;
      }

      if (a_region != 0 && a_region != region) {
        touches_other = true;
        break;
      }

      const auto& a_span = chf.spans[static_cast<std::size_t>(a_index)];
      const auto next_direction = (direction + 1) & 0x3;

      if (get_connection(a_span, next_direction) == static_cast<std::int32_t>(not_connected)) {
        continue;
      }

      const auto b_index = neighbor_span_index(chf, current.x + direction_offset_x(direction), current.z + direction_offset_z(direction), a_span, next_direction);

      if (chf.areas[static_cast<std::size_t>(b_index)] != area) {
        continue;
      }

      const auto b_region = src_regions[static_cast<std::size_t>(b_index)];

      if (b_region != 0 && b_region != region) {
        touches_other = true;
      }
    }

    if (touches_other) {
      src_regions[static_cast<std::size_t>(current.index)] = 0;
      continue;
    }

    ++count;

    for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
      if (get_connection(span, direction) == static_cast<std::int32_t>(not_connected)) {
        continue;
      }

      const auto a_index = neighbor_span_index(chf, current.x, current.z, span, direction);

      if (chf.areas[static_cast<std::size_t>(a_index)] != area) {
        continue;
      }

      if (distance[static_cast<std::size_t>(a_index)] >= lowest && src_regions[static_cast<std::size_t>(a_index)] == 0) {
        src_regions[static_cast<std::size_t>(a_index)] = region;
        src_distance[static_cast<std::size_t>(a_index)] = 0;
        stack.push_back(level_entry{current.x + direction_offset_x(direction), current.z + direction_offset_z(direction), a_index});
      }
    }
  }

  return count > 0;
}

/** Grows existing regions into the unassigned spans of @p stack (or of every span at or above @p level when @p fill_stack), nearest region first. */
auto expand_regions(std::int32_t max_iterations, std::uint16_t level, const compact_heightfield& chf, const std::pmr::vector<std::uint16_t>& distance, std::pmr::vector<std::uint16_t>& src_regions, std::pmr::vector<std::uint16_t>& src_distance, level_stack& stack, bool fill_stack, bake_arena& arena) -> void {
  if (fill_stack) {
    stack.clear();

    for (auto z = std::int32_t{0}; z < chf.height; ++z) {
      for (auto x = std::int32_t{0}; x < chf.width; ++x) {
        const auto& cell = chf.cells[static_cast<std::size_t>(x + z * chf.width)];

        for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
          if (distance[static_cast<std::size_t>(i)] >= level && src_regions[static_cast<std::size_t>(i)] == 0 && chf.areas[static_cast<std::size_t>(i)] != null_area) {
            stack.push_back(level_entry{x, z, i});
          }
        }
      }
    }
  } else {
    for (auto& entry : stack) {
      if (src_regions[static_cast<std::size_t>(entry.index)] != 0) {
        entry.index = -1;
      }
    }
  }

  struct dirty_entry {
    std::int32_t index;
    std::uint16_t region;
    std::uint16_t distance;
  }; // struct dirty_entry

  auto dirty = std::pmr::vector<dirty_entry>{arena.temp()};
  auto iteration = std::int32_t{0};

  while (!stack.empty()) {
    auto failed = std::size_t{0};
    dirty.clear();

    for (auto& entry : stack) {
      if (entry.index < 0) {
        ++failed;
        continue;
      }

      const auto& span = chf.spans[static_cast<std::size_t>(entry.index)];
      const auto area = chf.areas[static_cast<std::size_t>(entry.index)];

      auto region = src_regions[static_cast<std::size_t>(entry.index)];
      auto best_distance = std::uint16_t{0xffff};

      for (auto direction = std::int32_t{0}; direction < 4; ++direction) {
        if (get_connection(span, direction) == static_cast<std::int32_t>(not_connected)) {
          continue;
        }

        const auto a_index = neighbor_span_index(chf, entry.x, entry.z, span, direction);
        const auto a_region = src_regions[static_cast<std::size_t>(a_index)];

        if (chf.areas[static_cast<std::size_t>(a_index)] != area || a_region == 0 || (a_region & border_region)) {
          continue;
        }

        if (src_distance[static_cast<std::size_t>(a_index)] + 2 < best_distance) {
          region = a_region;
          best_distance = static_cast<std::uint16_t>(src_distance[static_cast<std::size_t>(a_index)] + 2);
        }
      }

      if (region) {
        dirty.push_back(dirty_entry{entry.index, region, best_distance});
        entry.index = -1;
      } else {
        ++failed;
      }
    }

    for (const auto& entry : dirty) {
      src_regions[static_cast<std::size_t>(entry.index)] = entry.region;
      src_distance[static_cast<std::size_t>(entry.index)] = entry.distance;
    }

    if (failed == stack.size()) {
      break;
    }

    if (level > 0 && ++iteration >= max_iterations) {
      break;
    }
  }
}

/** Buckets unassigned spans by distance level, two levels per stack, starting at @p start_level. */
auto sort_cells_by_level(std::uint16_t start_level, const compact_heightfield& chf, const std::pmr::vector<std::uint16_t>& distance, const std::pmr::vector<std::uint16_t>& src_regions, std::span<level_stack> stacks) -> void {
  constexpr auto log_levels_per_stack = 1;

  const auto start = static_cast<std::int32_t>(start_level) >> log_levels_per_stack;

  for (auto& stack : stacks) {
    stack.clear();
  }

  for (auto z = std::int32_t{0}; z < chf.height; ++z) {
    for (auto x = std::int32_t{0}; x < chf.width; ++x) {
      const auto& cell = chf.cells[static_cast<std::size_t>(x + z * chf.width)];

      for (auto i = static_cast<std::int32_t>(cell.index), ni = static_cast<std::int32_t>(cell.index + cell.count); i < ni; ++i) {
        if (chf.areas[static_cast<std::size_t>(i)] == null_area || src_regions[static_cast<std::size_t>(i)] != 0) {
          continue;
        }

        const auto stack_index = std::max(start - (static_cast<std::int32_t>(distance[static_cast<std::size_t>(i)]) >> log_levels_per_stack), 0);

        if (stack_index < static_cast<std::int32_t>(stacks.size())) {
          stacks[static_cast<std::size_t>(stack_index)].push_back(level_entry{x, z, i});
        }
      }
    }
  }
}

[[nodiscard]] auto build_regions_watershed(compact_heightfield& chf, std::int32_t min_region_area, std::int32_t merge_region_area, bake_arena& arena) -> bool {
  constexpr auto stack_count = std::size_t{8u};
  constexpr auto expand_iterations = std::int32_t{8};
  constexpr auto max_region_id = std::uint16_t{border_region - 1u};

  const auto span_count = chf.spans.size();

  auto raw_distance = std::pmr::vector<std::uint16_t>(span_count, arena.temp());
  auto distance = std::pmr::vector<std::uint16_t>(span_count, arena.temp());
  const auto max_distance = calculate_distance_field(chf, raw_distance);
  blur_distance_field(chf, 1, raw_distance, distance);

  auto src_regions = std::pmr::vector<std::uint16_t>(span_count, std::uint16_t{0}, arena.temp());
  auto src_distance = std::pmr::vector<std::uint16_t>(span_count, std::uint16_t{0}, arena.temp());

  auto stacks = std::array<level_stack, stack_count>{level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}, level_stack{arena.temp()}};
  auto flood_stack = level_stack{arena.temp()};

  auto region_id = std::uint16_t{1};
  auto level = static_cast<std::uint16_t>((max_distance + 1) & ~1);
  auto stack_index = std::size_t{stack_count - 1u};

  chf.border_size = 0;

  // Flood from the deepest (most open) spans downwards, two distance levels at a time, so regions start at the centres of open areas.
  while (level > 0) {
    level = static_cast<std::uint16_t>(level >= 2 ? level - 2 : 0);
    stack_index = (stack_index + 1u) % stack_count;

    if (stack_index == 0u) {
      sort_cells_by_level(level, chf, distance, src_regions, stacks);
    } else {
      for (const auto& entry : stacks[stack_index - 1u]) {
        if (entry.index >= 0 && src_regions[static_cast<std::size_t>(entry.index)] == 0) {
          stacks[stack_index].push_back(entry);
        }
      }
    }

    expand_regions(expand_iterations, level, chf, distance, src_regions, src_distance, stacks[stack_index], false, arena);

    for (const auto& entry : stacks[stack_index]) {
      if (entry.index < 0 || src_regions[static_cast<std::size_t>(entry.index)] != 0) {
        continue;
      }

      if (flood_region(entry.x, entry.z, entry.index, level, region_id, chf, distance, src_regions, src_distance, flood_stack)) {
        if (region_id == max_region_id) {
          return false;
        }

        ++region_id;
      }
    }
  }

  expand_regions(expand_iterations * 8, 0, chf, distance, src_regions, src_distance, flood_stack, true, arena);

  chf.max_regions = region_id;

  merge_and_filter_regions(chf, min_region_area, merge_region_area, src_regions, region_id, arena);

  for (auto i = std::size_t{0}; i < span_count; ++i) {
    chf.spans[i].region_id = src_regions[i];
  }

  return true;
}

} // namespace sbx::physics
