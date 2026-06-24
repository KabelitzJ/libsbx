// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENT_SERIALIZER_HPP_
#define LIBSBX_SCENES_COMPONENT_SERIALIZER_HPP_

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/scenes/scene_graph.hpp>

namespace sbx::scenes {

/**
 * @brief Registry of per-component-type YAML (de)serializers used by scene save/load.
 *
 * Each serializer pushes any asset UUIDs it references into the asset set so the scene can emit its dependency manifest. Structural data (id, tag, parent, transform) is handled by the scene itself, not here.
 */
class component_serializer {

public:

  using asset_set = std::unordered_set<math::uuid>;

  template<typename Component>
  auto register_component(std::string key, std::function<void(YAML::Node&, const Component&, asset_set&)> serialize, std::function<void(const YAML::Node&, scene_graph&, node)> deserialize) -> void {
    _entries.push_back(entry{
      .key = std::move(key),
      .has = [](const scene_graph& graph, const node n) -> bool {
        return graph.has_component<Component>(n);
      },
      .serialize = [serialize = std::move(serialize)](YAML::Node& out, const scene_graph& graph, const node n, asset_set& assets) -> void {
        serialize(out, graph.get_component<Component>(n), assets);
      },
      .deserialize = std::move(deserialize)
    });
  }

  auto serialize(YAML::Node& components, const scene_graph& graph, const node n, asset_set& assets) const -> void {
    for (const auto& entry : _entries) {
      if (entry.has(graph, n)) {
        auto component_node = YAML::Node{};

        entry.serialize(component_node, graph, n, assets);

        components[entry.key] = component_node;
      }
    }
  }

  auto deserialize(const YAML::Node& components, scene_graph& graph, const node n) const -> void {
    if (!components) {
      return;
    }

    for (const auto& entry : _entries) {
      if (const auto component_node = components[entry.key]; component_node) {
        entry.deserialize(component_node, graph, n);
      }
    }
  }

private:

  struct entry {
    std::string key;
    std::function<bool(const scene_graph&, node)> has;
    std::function<void(YAML::Node&, const scene_graph&, node, asset_set&)> serialize;
    std::function<void(const YAML::Node&, scene_graph&, node)> deserialize;
  }; // struct entry

  std::vector<entry> _entries;

}; // class component_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENT_SERIALIZER_HPP_
