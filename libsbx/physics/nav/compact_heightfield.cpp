// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/compact_heightfield.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

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

      for (auto& value : src_regions) {
        if (value == id) {
          value = best_neighbor;
        }
      }

      merged_any = true;
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

} // namespace sbx::physics
