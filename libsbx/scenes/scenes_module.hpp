// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_MODULE_HPP_
#define LIBSBX_SCENES_SCENE_MODULE_HPP_

#include <memory>
#include <unordered_map>
#include <utility>
#include <filesystem>

#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/scenes/asset_registry.hpp>
#include <libsbx/scenes/component_io.hpp>
#include <libsbx/scenes/asset_io.hpp>
#include <libsbx/scenes/scene.hpp>

namespace sbx::scenes {

class scenes_module final : public core::module<scenes_module> {

  inline static const auto is_registered = register_module(stage::normal);

public:

  struct line {
    sbx::math::vector4 position;
    sbx::math::color color;
  }; // struct line

  scenes_module();

  ~scenes_module() override;

  auto update() -> void override;

  auto create_scene(const std::string& name = "Scene") -> scenes::scene&;

  auto load_scene(const utility::hashed_string& name, const std::filesystem::path& path) -> scenes::scene&;

  auto close_scene(const utility::hashed_string& name) -> void;

  auto set_active_scene(const utility::hashed_string& name) -> void;

  auto active_scene() -> scenes::scene& {
    return *_active_scene;
  }

  auto active_scene() const -> const scenes::scene& {
    return *_active_scene;
  }

  auto find_scene(const utility::hashed_string& name) -> memory::observer_ptr<scenes::scene> {
    if (auto entry = _scenes.find(name); entry != _scenes.end()) {
      return entry->second.get();
    }

    return nullptr;
  }

  auto has_active_scene() const -> bool {
    return _active_scene != nullptr;
  }

  auto scene_count() const -> std::size_t {
    return _scenes.size();
  }

  auto save_scene(const std::filesystem::path& path) -> void {
    if (_active_scene) {
      _active_scene->save(path);
    }
  }

  auto set_scene_viewport(std::string name) -> void {
    _scene_viewport = std::move(name);
  }

  auto scene_viewport() const -> const std::string& {
    return _scene_viewport;
  }

  auto asset_registry() -> scenes::asset_registry& {
    return _asset_registry;
  }

  auto asset_registry() const -> const scenes::asset_registry& {
    return _asset_registry;
  }

  auto get_component_io_registry() -> component_io_registry& {
    return _component_io_registry;
  }

  auto get_asset_io_registry() -> asset_io_registry& {
    return _asset_io_registry;
  }

  auto get_component_io(const std::uint32_t id) -> scenes::component_io& {
    return _component_io_registry.get(id);
  }

  auto has_component_io(const std::uint32_t id) const -> bool {
    return _component_io_registry.has(id);
  }

  auto debug_lines() const -> const std::vector<line>&;

  auto clear_debug_lines() -> void;

  auto add_debug_line(const sbx::math::vector3& start, const sbx::math::vector3& end, const sbx::math::color& color) -> void;

  auto add_coordinate_arrows(const math::matrix4x4& transform, std::float_t length = 2.0f) -> void;

  auto add_debug_plane(const sbx::math::vector3& origin, const sbx::math::vector3& v1, const sbx::math::vector3& v2, std::uint32_t n1, std::uint32_t n2, std::float_t s1, std::float_t s2, const sbx::math::color& color, const sbx::math::color& outline) -> void;

  auto add_debug_volume(const math::matrix4x4& matrix, const math::volume& volume, const sbx::math::color& color) -> void;

  auto add_debug_box(const math::matrix4x4& matrix, const math::volume& volume, const sbx::math::color& color) -> void;

  auto add_debug_circle(const math::vector3& center, const std::float_t radius, const math::vector3& normal, const math::color& color, const std::uint32_t segments = 32) -> void;

  auto add_debug_sphere(const math::vector3& center, const std::float_t radius, const math::color& color, const std::uint32_t segments = 32) -> void;

  auto add_debug_frustum(const math::matrix4x4& view, const math::matrix4x4& projection, const sbx::math::color& color) -> void;

private:

  scenes::asset_registry _asset_registry;
  scenes::component_io_registry _component_io_registry;
  scenes::asset_io_registry _asset_io_registry;

  std::unordered_map<utility::hashed_string, std::unique_ptr<scenes::scene>> _scenes;
  memory::observer_ptr<scenes::scene> _active_scene{nullptr};

  std::vector<line> _debug_lines{};

  std::string _scene_viewport{std::string{graphics::viewport::window_name}};

}; // class scenes_module

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_MODULE_HPP_
