// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components/camera.hpp>

namespace sbx::scenes {

scene::scene(const std::filesystem::path& path, component_io_registry& component_io)
: _graph{},
  _environment{_graph, scenes::node::null, math::vector3{-1.0f, -1.0f, -1.0f}, math::color{1.0f, 1.0f, 1.0f, 1.0f}},
  _serializer{component_io} {
  // Create camera node
  auto camera_node = _graph.create_node("CAMERA");

  auto& devices_module = core::engine::get_module<devices::devices_module>();
  auto& window = devices_module.window();

  _graph.add_component<scenes::camera>(camera_node, math::angle{math::degree{60.0f}}, window.aspect_ratio(), 0.1f, 1000.0f);

  _environment.set_active_camera(camera_node);

  // Load scene from disk
  _name = _serializer.load(path, _graph, _assets);
}

auto scene::save(const std::filesystem::path& path) -> void {
  _serializer.save(path, _name, _graph, _assets);
}

} // namespace sbx::scenes
