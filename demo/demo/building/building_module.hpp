// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDINGS_BUILDING_MODULE_HPP_
#define DEMO_BUILDINGS_BUILDING_MODULE_HPP_

#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain/terrain_module.hpp>

#include <demo/building/building_definition.hpp>
#include <demo/building/building_instance.hpp>
#include <demo/building/road_types.hpp>
#include <demo/building/road_placement.hpp>
#include <demo/building/road_drawing.hpp>
#include <demo/building/zone_types.hpp>

namespace demo {

struct placement_preview {
  std::uint32_t definition_id{};
  std::int32_t origin_x{};
  std::int32_t origin_z{};
  orientation orient{orientation::n};
  bool valid{false};
}; // struct placement_preview

struct road_preview {
  bool active{false};
  bool has_start{false};
  cell_coordinates start{};
  cell_coordinates cursor{};
  road_type type{road_type::dirt};
  bool shift_held{false};
  std::vector<cell_coordinates> preview_cells;
  bool dirty{false};
}; // struct road_preview

struct zone_painting {
  bool active{false};
  zone_type current_type{zone_type::residential};
  std::int32_t brush_radius{0};
}; // struct zone_painting

class building_module final : public sbx::core::module<building_module> {

  inline static const auto is_registered = register_module(stage::normal);

public:

  building_module() = default;

  ~building_module() override = default;

  auto update() -> void override {
    
  }

  auto register_definition(building_definition definition) -> void {
    definition.precompute_footprints();

    auto id = definition.id;

    sbx::utility::logger<"demo">::info("Registered building definition '{}' (id: {}, footprint: {}x{})", definition.name, id, definition.footprint_width, definition.footprint_height);

    _definitions[id] = std::move(definition);
  }

  auto get_definition(std::uint32_t definition_id) const -> const building_definition* {
    auto entry = _definitions.find(definition_id);

    return (entry != _definitions.end()) ? &entry->second : nullptr;
  }

  auto can_place(std::uint32_t definition_id, std::int32_t origin_x, std::int32_t origin_z, orientation orient, std::float_t max_slope = 3.0f) const -> bool {
    auto* definition = get_definition(definition_id);

    if (!definition) {
      sbx::utility::logger<"buildings">::warn("Placement check failed: unknown definition id {}", definition_id);
      return false;
    }

    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
    auto& grid = terrain_module.grid();

    const auto& cells = definition->get_footprint(orient);

    for (const auto& offset : cells) {
      auto cell_x = origin_x + offset.x;
      auto cell_z = origin_z + offset.z;

      if (!grid.in_bounds(cell_x, cell_z)) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): out of bounds", cell_x, cell_z);
        return false;
      }

      const auto& grid_cell = grid.at(cell_x, cell_z);

      if (grid_cell.building_id != 0) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): occupied by building {}", cell_x, cell_z, grid_cell.building_id);
        return false;
      }

      if (grid_cell.road_type != 0) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): occupied by road", cell_x, cell_z);
        return false;
      }

      auto height = terrain_module.get_height_at_cell(cell_coordinates{cell_x, cell_z});

      if (height < terrain_constants::sea_level) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): below sea level", cell_x, cell_z);
        return false;
      }

      if (terrain_module.get_slope_at(cell_coordinates{cell_x, cell_z}) > max_slope) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): slope too steep", cell_x, cell_z);
        return false;
      }
    }

    return true;
  }

  auto place(std::uint32_t definition_id, std::int32_t origin_x, std::int32_t origin_z, orientation orient) -> std::uint32_t {
    auto* definition = get_definition(definition_id);

    if (!definition) {
      sbx::utility::logger<"demo">::error("Failed to place building: unknown definition id {}", definition_id);
      return 0;
    }

    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& grid = terrain_module.grid();
    auto& graph = scenes_module.scene().graph();
    auto& asset_registry = scenes_module.asset_registry();

    auto instance_id = _next_instance_id++;

    auto instance = building_instance{};
    instance.id = instance_id;
    instance.definition_id = definition_id;
    instance.origin_x = origin_x;
    instance.origin_z = origin_z;
    instance.orient = orient;
    instance.state = building_state::planned;

    const auto& cells = definition->get_footprint(orient);

    for (const auto& offset : cells) {
      auto cell_x = origin_x + offset.x;
      auto cell_z = origin_z + offset.z;

      auto& grid_cell = grid.at(cell_x, cell_z);
      grid_cell.building_id = static_cast<std::uint16_t>(instance_id);
    }

    _flatten_footprint(cells, origin_x, origin_z);
    _roads_dirty = true;

    // Compute footprint center in world space
    auto cell_sz = grid::cell_size;

    auto min_x = std::numeric_limits<std::int32_t>::max();
    auto max_x = std::numeric_limits<std::int32_t>::min();
    auto min_z = std::numeric_limits<std::int32_t>::max();
    auto max_z = std::numeric_limits<std::int32_t>::min();

    for (const auto& offset : cells) {
      min_x = std::min(min_x, origin_x + offset.x);
      max_x = std::max(max_x, origin_x + offset.x);
      min_z = std::min(min_z, origin_z + offset.z);
      max_z = std::max(max_z, origin_z + offset.z);
    }

    auto corner = terrain_module.cell_to_world(cell_coordinates{min_x, min_z});
    auto center_wx = corner.x + static_cast<std::float_t>(max_x - min_x + 1) * cell_sz * 0.5f;
    auto center_wz = corner.z + static_cast<std::float_t>(max_z - min_z + 1) * cell_sz * 0.5f;
    auto center_wy = terrain_module.get_height_at(world_coordinates{center_wx, center_wz});

    auto node = graph.create_node(definition->name);

    // Rotation from orientation (8 directions, 45° each)
    auto angle_degrees = static_cast<std::float_t>(static_cast<std::uint8_t>(orient)) * 45.0f;

    auto rotation = sbx::math::quaternion{sbx::math::vector3::up, sbx::math::angle{sbx::math::degree{angle_degrees}}};

    auto& transform = graph.get_component<sbx::scenes::transform>(node);
    transform.set_position(sbx::math::vector3{center_wx, center_wy, center_wz});
    transform.set_rotation(rotation);

    // Attach mesh
    auto mesh_handle = asset_registry.get_mesh(sbx::utility::hashed_string{definition->mesh_id});
    auto material_handle = asset_registry.get_material(sbx::utility::hashed_string{definition->material_id});
    graph.add_component<sbx::scenes::static_mesh>(node, mesh_handle, material_handle);

    instance.scene_node = node;

    _instances[instance_id] = std::move(instance);

    sbx::utility::logger<"demo">::info("Placed '{}' (instance: {}) at cell ({}, {}), orientation: {}", definition->name, instance_id, origin_x, origin_z, static_cast<std::uint8_t>(orient));

    return instance_id;
  }

  auto remove(std::uint32_t instance_id) -> void {
    auto entry = _instances.find(instance_id);

    if (entry == _instances.end()) {
      sbx::utility::logger<"demo">::warn("Cannot remove building: unknown instance id {}", instance_id);

      return;
    }

    auto& instance = entry->second;
    auto* definition = get_definition(instance.definition_id);

    if (!definition) {
      sbx::utility::logger<"demo">::error("Cannot remove building: instance {} has invalid definition id {}", instance_id, instance.definition_id);

      return;
    }

    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& grid = terrain_module.grid();
    auto& graph = scenes_module.scene().graph();

    const auto& cells = definition->get_footprint(instance.orient);

    for (const auto& offset : cells) {
      auto cell_x = instance.origin_x + offset.x;
      auto cell_z = instance.origin_z + offset.z;

      auto& grid_cell = grid.at(cell_x, cell_z);
      grid_cell.building_id = 0;
    }

    graph.destroy_node(instance.scene_node);

    _instances.erase(entry);

    sbx::utility::logger<"demo">::info("Removed '{}' (instance: {}) from cell ({}, {})", definition->name, instance_id, instance.origin_x, instance.origin_z);
  }

  auto update_preview(std::uint32_t definition_id, std::int32_t origin_x, std::int32_t origin_z, orientation orient) -> placement_preview {
    auto preview = placement_preview{};
    preview.definition_id = definition_id;
    preview.origin_x = origin_x;
    preview.origin_z = origin_z;
    preview.orient = orient;
    preview.valid = can_place(definition_id, origin_x, origin_z, orient);

    return preview;
  }

  auto get_instance(std::uint32_t instance_id) -> building_instance* {
    auto entry = _instances.find(instance_id);

    return (entry != _instances.end()) ? &entry->second : nullptr;
  }

  auto get_instance(std::uint32_t instance_id) const -> const building_instance* {
    auto entry = _instances.find(instance_id);

    return (entry != _instances.end()) ? &entry->second : nullptr;
  }

  auto instance_count() const -> std::size_t {
    return _instances.size();
  }

  template<typename Callable>
  auto for_each_instance(Callable&& callable) -> void {
    for (auto& [id, instance] : _instances) {
      std::invoke(callable, instance);
    }
  }

  template<typename Callable>
  auto for_each_instance(Callable&& callable) const -> void {
    for (const auto& [id, instance] : _instances) {
      std::invoke(callable, instance);
    }
  }

  auto place_road(std::int32_t start_x, std::int32_t start_z, std::int32_t end_x, std::int32_t end_z, road_type type) -> road_placement_result {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto result = place_road_line(terrain_module.grid(), start_x, start_z, end_x, end_z, type);

    if (!result.placed_cells.empty()) {
      _roads_dirty = true;
    }

    return result;
  }

  auto place_road_path(const std::vector<cell_coordinates>& cells, road_type type) -> road_placement_result {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto result = demo::place_road_path(terrain_module.grid(), cells, type);

    if (!result.placed_cells.empty()) {
      _roads_dirty = true;
    }

    return result;
  }

  auto remove_road_at(std::int32_t cell_x, std::int32_t cell_z) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto dirty_chunks = remove_road(terrain_module.grid(), cell_x, cell_z);

    if (!dirty_chunks.empty()) {
      _roads_dirty = true;
    }
  }

  auto roads_dirty() const -> bool {
    return _roads_dirty;
  }

  auto clear_roads_dirty() -> void {
    _roads_dirty = false;
  }

  // ---- Road building mode ----

  auto enter_road_mode(road_type type) -> void {
    _road_preview_state.active = true;
    _road_preview_state.has_start = false;
    _road_preview_state.type = type;
    _road_preview_state.preview_cells.clear();
    _road_preview_state.dirty = true;
  }

  auto exit_road_mode() -> void {
    _road_preview_state.active = false;
    _road_preview_state.has_start = false;
    _road_preview_state.preview_cells.clear();
    _road_preview_state.dirty = true;
  }

  auto update_road_cursor(std::int32_t cell_x, std::int32_t cell_z, bool shift_held) -> void {
    _road_preview_state.cursor = {cell_x, cell_z};
    _road_preview_state.shift_held = shift_held;

    _road_preview_state.preview_cells.clear();

    if (_road_preview_state.has_start) {
      auto path = build_snapped_road_path(_road_preview_state.start, _road_preview_state.cursor, shift_held);
      _road_preview_state.preview_cells = std::move(path.cells);
    } else {
      _road_preview_state.preview_cells.push_back(_road_preview_state.cursor);
    }

    _road_preview_state.dirty = true;
  }

  auto confirm_road_point() -> road_placement_result {
    if (!_road_preview_state.active) {
      return {};
    }

    if (!_road_preview_state.has_start) {
      _road_preview_state.has_start = true;
      _road_preview_state.start = _road_preview_state.cursor;
      return {};
    }

    auto result = place_road_path(_road_preview_state.preview_cells, _road_preview_state.type);

    // Chain: next segment starts from current cursor
    _road_preview_state.start = _road_preview_state.cursor;
    _road_preview_state.preview_cells.clear();
    _road_preview_state.preview_cells.push_back(_road_preview_state.cursor);
    _road_preview_state.dirty = true;

    return result;
  }

  auto get_road_preview() const -> const road_preview& {
    return _road_preview_state;
  }

  auto is_road_mode() const -> bool {
    return _road_preview_state.active;
  }

  auto road_preview_dirty() const -> bool {
    return _road_preview_state.dirty;
  }

  auto clear_road_preview_dirty() -> void {
    _road_preview_state.dirty = false;
  }

  // ---- Zone painting mode ----

  auto enter_zone_mode(zone_type type) -> void {
    _zone_painting_state.active = true;
    _zone_painting_state.current_type = type;

    sbx::utility::logger<"demo">::info("Zone mode entered: {}", get_zone_name(type));
  }

  auto exit_zone_mode() -> void {
    _zone_painting_state.active = false;

    sbx::utility::logger<"demo">::info("Zone mode exited");
  }

  auto set_zone_type(zone_type type) -> void {
    _zone_painting_state.current_type = type;
  }

  auto set_zone_brush_radius(std::int32_t radius) -> void {
    _zone_painting_state.brush_radius = radius;
  }

  auto paint_zone(std::int32_t cell_x, std::int32_t cell_z) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
    auto& grid = terrain_module.grid();

    auto radius = _zone_painting_state.brush_radius;

    for (auto dz = -radius; dz <= radius; ++dz) {
      for (auto dx = -radius; dx <= radius; ++dx) {
        auto cx = cell_x + dx;
        auto cz = cell_z + dz;

        if (!grid.in_bounds(cx, cz)) {
          continue;
        }

        auto& cell = grid.at(cx, cz);

        // Don't paint over buildings or roads
        if (cell.building_id != 0 || cell.road_type != 0) {
          continue;
        }

        // Don't paint underwater
        auto height = terrain_module.get_height_at_cell(cell_coordinates{cx, cz});

        if (height < terrain_constants::sea_level) {
          continue;
        }

        cell.zone_type = static_cast<std::uint8_t>(_zone_painting_state.current_type);
      }
    }

    _zones_dirty = true;

    sbx::utility::logger<"demo">::debug("Painted zone {} at ({}, {}) radius {}", get_zone_name(_zone_painting_state.current_type), cell_x, cell_z, radius);
  }

  auto erase_zone(std::int32_t cell_x, std::int32_t cell_z) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
    auto& grid = terrain_module.grid();

    auto radius = _zone_painting_state.brush_radius;

    for (auto dz = -radius; dz <= radius; ++dz) {
      for (auto dx = -radius; dx <= radius; ++dx) {
        auto cx = cell_x + dx;
        auto cz = cell_z + dz;

        if (!grid.in_bounds(cx, cz)) {
          continue;
        }

        grid.at(cx, cz).zone_type = 0;
      }
    }

    _zones_dirty = true;
  }

  auto is_zone_mode() const -> bool {
    return _zone_painting_state.active;
  }

  auto get_zone_painting() const -> const zone_painting& {
    return _zone_painting_state;
  }

  auto zones_dirty() const -> bool {
    return _zones_dirty;
  }

  auto clear_zones_dirty() -> void {
    _zones_dirty = false;
  }

private:

  auto _flatten_footprint(const footprint& cells, std::int32_t origin_x, std::int32_t origin_z) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto average_height = 0.0f;

    for (const auto& offset : cells) {
      auto cell_x = origin_x + offset.x;
      auto cell_z = origin_z + offset.z;
      auto world = terrain_module.cell_to_world(cell_coordinates{cell_x, cell_z});

      average_height += terrain_module.get_height_at(world);
    }

    average_height /= static_cast<std::float_t>(cells.size());

    auto& height_map = terrain_module.heightmap();

    for (const auto& offset : cells) {
      auto vertex_x = origin_x + offset.x;
      auto vertex_z = origin_z + offset.z;

      height_map.set_height(vertex_x, vertex_z, average_height);
      height_map.set_height(vertex_x + 1, vertex_z, average_height);
      height_map.set_height(vertex_x, vertex_z + 1, average_height);
      height_map.set_height(vertex_x + 1, vertex_z + 1, average_height);
    }
  }

  std::unordered_map<std::uint32_t, building_definition> _definitions;
  std::unordered_map<std::uint32_t, building_instance> _instances;
  std::uint32_t _next_instance_id{1};

  bool _roads_dirty{false};
  bool _zones_dirty{false};

  road_preview _road_preview_state;
  zone_painting _zone_painting_state;

}; // class building_module

} // namespace demo

#endif // DEMO_BUILDINGS_BUILDING_MODULE_HPP_
