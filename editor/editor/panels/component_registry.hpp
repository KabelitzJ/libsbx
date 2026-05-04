// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_COMPONENT_REGISTRY_HPP_
#define EDITOR_PANELS_COMPONENT_REGISTRY_HPP_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libsbx/core/engine.hpp>

#include <libsbx/ecs/registry.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

struct component_entry {
  std::uint32_t id;
  std::string label;
  bool is_removable;
  std::function<bool(sbx::scenes::node)> has;
  std::function<void(sbx::scenes::node)> draw;
  std::function<void(sbx::scenes::node)> remove;
  std::function<bool(sbx::scenes::node)> factory;
}; // struct component_entry

class component_registry {

public:

  component_registry() = default;

  template<typename Type, typename Draw>
  auto register_component(std::string label, bool is_removable, Draw&& draw) -> component_registry& {
    const auto id = sbx::ecs::type_id<Type>::value();

    auto entry = component_entry{
      .id = id,
      .label = std::move(label),
      .is_removable = is_removable,
      .has = [](sbx::scenes::node node) -> bool {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto& scene = scenes_module.active_scene();
        auto& graph = scene.graph();

        return graph.has_component<Type>(node);
      },
      .draw = [draw_fn = std::forward<Draw>(draw)](sbx::scenes::node node) -> void {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto& scene = scenes_module.active_scene();
        auto& graph = scene.graph();

        auto& component = graph.get_component<Type>(node);

        std::invoke(draw_fn, node, component);
      },
      .remove = [](sbx::scenes::node node) -> void {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto& scene = scenes_module.active_scene();
        auto& graph = scene.graph();

        graph.remove_component<Type>(node);
      },
      .factory = {}
    };

    _by_id[id] = _entries.size();
    _entries.push_back(std::move(entry));

    return *this;
  }

  template<typename Type, typename Draw, typename Factory>
  auto register_component(std::string label, bool is_removable, Draw&& draw, Factory&& factory) -> component_registry& {
    register_component<Type>(std::move(label), is_removable, std::forward<Draw>(draw));

    _entries.back().factory = [fn = std::forward<Factory>(factory)](sbx::scenes::node node) -> bool {
      auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
      auto& scene = scenes_module.active_scene();
      auto& graph = scene.graph();

      if (graph.has_component<Type>(node)) {
        return false;
      }

      graph.add_component<Type>(node, std::invoke(fn));

      return true;
    };

    return *this;
  }

  auto entries() const -> const std::vector<component_entry>& {
    return _entries;
  }

  auto find(std::uint32_t id) const -> const component_entry* {
    if (auto entry = _by_id.find(id); entry != _by_id.end()) {
      return &_entries[entry->second];
    }

    return nullptr;
  }

private:

  std::vector<component_entry> _entries;
  std::unordered_map<std::uint32_t, std::size_t> _by_id;

}; // class component_registry

} // namespace editor

#endif // EDITOR_PANELS_COMPONENT_REGISTRY_HPP_
