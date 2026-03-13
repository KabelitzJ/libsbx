// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_
#define LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_

#include <deque>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
#include <ImGuizmo.h>

#include <libsbx/editor/bindings/imgui.hpp>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/version.hpp>
#include <libsbx/core/profiler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/tag.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/graphics_module.hpp>

// #include <libsbx/models/static_mesh_subrenderer.hpp>

#include <libsbx/editor/pipeline.hpp>
#include <libsbx/editor/themes.hpp>
#include <libsbx/editor/fonts.hpp>
#include <libsbx/editor/dialog.hpp>
#include <libsbx/editor/menu_bar.hpp>

#include <libsbx/editor/hierarchy_panel.hpp>
#include <libsbx/editor/profiler_panel.hpp>
#include <libsbx/editor/nodes_panel.hpp>
#include <libsbx/editor/log_panel.hpp>
#include <libsbx/editor/property_panel.hpp>

namespace sbx::editor {

class editor_subrenderer final : public sbx::graphics::subrenderer {

  inline static constexpr auto ini_file = std::string_view{"res://data/imgui.ini"};

  using base = sbx::graphics::subrenderer;

  std::string _actual_ini_file;

public:

  editor_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name);

  ~editor_subrenderer() override;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

private:

  enum class popup : std::uint16_t {
    hierarchy_add_new_node = utility::bit_v<0u>,
    hierarchy_add_component = utility::bit_v<1u>,
    hierarchy_delete = utility::bit_v<2u>,
    menu_preferences = utility::bit_v<3u>,
    menu_about = utility::bit_v<4u>,
  }; // enum class popup

  auto _setup_dockspace() -> void;

  auto _setup_windows() -> void;

  auto _save() -> void;

  auto _load() -> void;

  auto _undo() -> void;

  auto _draw_scene_toolbar() -> void;
  auto _draw_scene_gizmo(const ImVec2& viewport_min, const ImVec2& viewport_size) -> void;

  static auto _to_imguizmo_matrix(const sbx::math::matrix4x4& in, std::span<std::float_t, 16> out) -> void;
  static auto _from_imguizmo_matrix(std::span<const std::float_t, 16> in) -> sbx::math::matrix4x4;

  std::string _attachment_name;

  pipeline _pipeline;
  graphics::descriptor_handler _descriptor_handler;

  hierarchy_panel _hierarchy_panel;
  profiler_panel _profiler_panel;
  nodes_panel _nodes_panel;
  log_panel _log_panel;
  property_panel _property_panel;

  utility::bit_field<popup> _open_popups;

  math::vector2u _viewport_size;
  math::vector2 _mouse_position;

  std::vector<editor::menu> _menu;

  editor::themes _editor_theme;
  editor::fonts _editor_font;

  std::int32_t _gizmo_operation{ImGuizmo::OPERATION::TRANSLATE};
  std::int32_t _gizmo_mode{ImGuizmo::MODE::LOCAL};

}; // class editor_subrenderer

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_
