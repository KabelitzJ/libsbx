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

namespace demo {

struct placement_preview {
  std::uint32_t definition_id{};
  std::int32_t origin_x{};
  std::int32_t origin_z{};
  orientation orient{orientation::n};
  bool valid{false};
}; // struct placement_preview

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

      auto height = terrain_module.get_height_at_cell(cell_x, cell_z);

      if (height < terrain_constants::sea_level) {
        sbx::utility::logger<"buildings">::debug("Placement rejected at ({}, {}): below sea level", cell_x, cell_z);
        return false;
      }

      if (terrain_module.get_slope_at(cell_x, cell_z) > max_slope) {
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

    auto [world_x, world_z] = terrain_module.cell_to_world(origin_x, origin_z);
    auto world_y = terrain_module.get_height_at(world_x, world_z);

    auto node = graph.create_node(definition->name);
    auto& transform = graph.get_component<sbx::scenes::transform>(node);
    transform.set_position(sbx::math::vector3{world_x, world_y, world_z});

    // @todo: set rotation from orientation
    // @todo: attach mesh component from definition->mesh_id

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

  auto place_road(std::int32_t start_x, std::int32_t start_y, std::int32_t end_x, std::int32_t end_y, road_type type) -> road_placement_result {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto result = place_road_line(terrain_module.grid(), start_x, start_y, end_x, end_y, type);

    if (!result.placed_cells.empty()) {
      _roads_dirty = true;
    }

    return result;
  }

  auto remove_road_at(std::int32_t cell_x, std::int32_t cell_y) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto dirty_chunks = remove_road(terrain_module.grid(), cell_x, cell_y);

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

private:

  auto _flatten_footprint(const footprint& cells, std::int32_t origin_x, std::int32_t origin_z) -> void {
    auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

    auto average_height = 0.0f;

    for (const auto& offset : cells) {
      auto cell_x = origin_x + offset.x;
      auto cell_z = origin_z + offset.z;
      auto [world_x, world_z] = terrain_module.cell_to_world(cell_x, cell_z);

      average_height += terrain_module.get_height_at(world_x, world_z);
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

}; // class building_module

} // namespace demo

#endif // DEMO_BUILDINGS_BUILDING_MODULE_HPP_
