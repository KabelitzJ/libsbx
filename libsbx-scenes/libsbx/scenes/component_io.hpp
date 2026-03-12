// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENT_IO_HPP_
#define LIBSBX_SCENES_COMPONENT_IO_HPP_

#include <functional>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <libsbx/ecs/registry.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/scene_asset_table.hpp>

namespace sbx::scenes {

struct component_io {
  std::string name;
  std::function<void(YAML::Emitter&, scene_graph&, scene_asset_table&, const node)> save;
  std::function<void(const YAML::Node&, scene_graph&, scene_asset_table&, const node)> load;
}; // struct component_io

class component_io_registry {

public:

  template<typename Type, std::invocable<YAML::Emitter&, scene_graph&, scene_asset_table&, const Type&> Save, std::invocable<const YAML::Node&, scene_graph&, scene_asset_table&> Load>
  auto register_component(const std::string& name, Save&& save, Load&& load) -> void {
    const auto id = ecs::type_id<Type>::value();

    _by_name[name] = id;

    _by_id[id] = component_io{
      .name = name,
      .save = [s = std::forward<Save>(save)](YAML::Emitter& emitter, scene_graph& graph, scene_asset_table& assets, const node node) -> void {
        const auto& component = graph.get_component<Type>(node);

        std::invoke(s, emitter, graph, assets, component);
      },
      .load = [l = std::forward<Load>(load)](const YAML::Node& yaml, scene_graph& graph, scene_asset_table& assets, const node node) -> void {
        graph.add_or_update_component<Type>(node, std::invoke(l, yaml, graph, assets));
      }
    };
  }

  auto get(const std::uint32_t id) -> component_io& {
    return _by_id.at(id);
  }

  auto get(const std::string& name) -> component_io& {
    return _by_id.at(_by_name.at(name));
  }

  auto has(const std::uint32_t id) const -> bool {
    return _by_id.contains(id);
  }

  auto has(const std::string& name) const -> bool {
    return _by_name.contains(name) && _by_id.contains(_by_name.at(name));
  }

private:

  std::unordered_map<std::uint32_t, component_io> _by_id;
  std::unordered_map<std::string, std::uint32_t> _by_name;

}; // class component_io_registry

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENT_IO_HPP_
