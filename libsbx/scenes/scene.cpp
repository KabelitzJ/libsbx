// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/id.hpp>

namespace sbx::scenes {

scene::scene(const std::filesystem::path& path, component_io_registry& component_io, asset_io_registry& asset_io, asset_registry& registry)
: _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 1.0f, 1.0f, 1.0f}},
  _serializer{component_io, asset_io, registry} {
  const auto data = _serializer.load(path, _graph);

  _name = data.name;

  auto camera_node = scenes::node::null;

  if (data.camera_id != math::uuid::nil()) {
    camera_node = _graph.find_node(scenes::id{data.camera_id});
  }

  if (camera_node == scenes::node::null) {
    auto view = _graph.query<scenes::camera>();

    if (view.begin() != view.end()) {
      camera_node = *view.begin();
    }
  }

  if (camera_node == scenes::node::null) {
    camera_node = _graph.create_node("CAMERA");

    utility::logger<"scenes">::debug("No camera node found, creating default");
  }

  if (!_graph.has_component<scenes::camera>(camera_node)) {
    _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, 0.1f, 1000.0f);
  }

  _environment.set_active_camera(camera_node);
}

scene::scene(component_io_registry& component_io, asset_io_registry& asset_io, asset_registry& registry, const std::string& name)
: _name{name},
  _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 1.0f, 1.0f, 1.0f}},
  _serializer{component_io, asset_io, registry} {
  auto camera_node = _graph.create_node("Camera");

  _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, 0.1f, 1000.0f);

  _environment.set_active_camera(camera_node);
}

auto scene::save(const std::filesystem::path& path) -> void {
  _serializer.save(path, _name, _graph, _environment.camera());
}

} // namespace sbx::scenes
