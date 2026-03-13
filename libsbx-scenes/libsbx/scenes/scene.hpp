// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_HPP_
#define LIBSBX_SCENES_SCENE_HPP_

#include <filesystem>
#include <string>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/scene_environment.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

namespace sbx::scenes {

struct node_destroyed_event {
  scenes::node node;
}; // struct node_destroyed_event

template<typename Component>
struct component_removed_event {
  scenes::node node;
}; // struct component_removed_event

class scene {

public:

  scene(const std::filesystem::path& path, component_io_registry& component_io, asset_io_registry& asset_io, asset_registry& registry);

  virtual ~scene() = default;

  auto graph() -> scene_graph& {
    return _graph;
  }

  auto graph() const -> const scene_graph& {
    return _graph;
  }

  auto environment() -> scene_environment& {
    return _environment;
  }

  auto environment() const -> const scene_environment& {
    return _environment;
  }

  auto name() const -> const std::string& {
    return _name;
  }

  auto save(const std::filesystem::path& path) -> void;

private:

  std::string _name;
  scene_graph _graph;
  scene_environment _environment;
  scene_serializer _serializer;

}; // class scene

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_HPP_
