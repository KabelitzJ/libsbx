// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_MODULE_HPP_
#define LIBSBX_SCENES_SCENE_MODULE_HPP_

#include <memory>
#include <optional>
#include <utility>
#include <filesystem>

#include <libsbx/utility/memory_tracker.hpp>

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/scenes/scene.hpp>

namespace sbx::scenes {

class scenes_module final : public core::module<scenes_module> {

  friend class scene;

  inline static const auto is_registered = register_module(stage::normal);

public:

  struct line {
    sbx::math::vector4 position;
    sbx::math::color color;
  }; // struct line

  scenes_module();

  ~scenes_module() override;

  auto update() -> void override;

  auto load_scene(const std::filesystem::path& path) -> scenes::scene&;

  auto scene() -> scenes::scene&;

  auto save_scene(const std::filesystem::path& path) -> void {
    if (_scene) {
      _scene->save(path);
    }
  }

  template<typename Type, std::invocable<YAML::Node&, const Type&> Save, std::invocable<YAML::Node&> Load>
  auto register_component(const std::string& name, Save&& save, Load&& load) -> void {
    _component_io_registry.register_component<Type>(name, std::forward<Save>(save), std::forward<Load>(load));
  }

  auto component_io(const std::uint32_t id) -> component_io&;

  auto has_component_io(const std::uint32_t id) -> bool;

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

  utility::tracker_scope _memory_tracker_scope{"scenes", "scenes_module"};

  std::optional<scenes::scene> _scene;

  component_io_registry _component_io_registry;

  std::vector<line> _debug_lines{};

}; // class scene_modules

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_MODULE_HPP_
