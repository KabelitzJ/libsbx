// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/id.hpp>

namespace sbx::scenes {

scene::scene(const std::filesystem::path& path, component_io_registry& component_io, asset_io_registry& asset_io)
: _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 1.0f, 1.0f, 1.0f}},
  _serializer{component_io, asset_io} {
  // Load scene from disk
  const auto data = _serializer.load(path, _graph, _assets);

  _name = data.name;

  // Resolve camera node from scene data
  auto camera_node = scenes::node::null;

  if (data.camera_id != math::uuid::null()) {
    camera_node = _graph.find_node(scenes::id{data.camera_id});
  }

  // If the scene file didn't specify a camera, create a default one
  if (camera_node == scenes::node::null) {
    camera_node = _graph.create_node("CAMERA");

    utility::logger<"scenes">::debug("No camera node in scene file, creating default");
  }

  // Ensure camera component exists
  if (!_graph.has_component<scenes::camera>(camera_node)) {
    auto& devices_module = core::engine::get_module<devices::devices_module>();
    auto& window = devices_module.window();

    _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, window.aspect_ratio(), 0.1f, 1000.0f);
  }

  _environment.set_active_camera(camera_node);
}

auto scene::save(const std::filesystem::path& path) -> void {
  _serializer.save(path, _name, _graph, _assets, _environment.camera());
}

} // namespace sbx::scenes
