// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LAUNCHER_LAUNCHER_MODULE_HPP_
#define LAUNCHER_LAUNCHER_MODULE_HPP_

#include <array>
#include <filesystem>
#include <string_view>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/projects_module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/render_module.hpp>
#include <libsbx/render/ui/ui_layer.hpp>
#include <libsbx/render/ui/widgets/file_dialog.hpp>

namespace launcher {

/**
 * @brief The Project Manager window: title, the recent-projects list (sbx::core::projects_module),
 * and New/Open Project — draws it all as one sbx::render::ui_layer, registered with
 * render_module::ui() the same way editor_ui_layer is (see the project plan's §1/§3). Picking or
 * creating a project spawns the editor as a new process (`--project <root>`, see
 * filesystem::executable_directory) and quits the launcher — it never actually opens the project
 * itself, projects_module::open()/create() here exist only to update the recents list.
 *
 * Instantiates render_module (and, transitively, assets_module/scenes_module — see
 * render_module::dependencies) purely to reach ui_system via render_module::ui(); it never
 * renders a 3D scene (no camera is ever created). This is the "cheapest, zero rework" option the
 * project plan's §1 left open when it promoted ImGui/render_graph into libsbx — see there before
 * changing this.
 */
class launcher_module final : public sbx::utility::noncopyable, public sbx::render::ui_layer {

public:

  using dependencies = sbx::core::dependency_list<sbx::platform::platform_module, sbx::graphics::graphics_module, sbx::render::render_module, sbx::core::projects_module>;

  launcher_module();

  ~launcher_module();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Launcher";
  }

  auto build() -> void override;

private:

  enum class pending_pick {
    none,
    open_project,
    new_project_parent
  }; // enum class pending_pick

  auto _draw_recent_projects() -> void;

  auto _draw_new_project_name_dialog() -> void;

  /** @brief Records @p root as opened (updates recents), spawns `editor --project <root>` next to this binary, then quits. */
  auto _launch_editor(const std::filesystem::path& root) -> void;

  sbx::render::widgets::file_dialog _file_dialog{};
  pending_pick _pending_pick{pending_pick::none};

  bool _show_new_project_name_dialog{false};
  std::filesystem::path _new_project_parent{};
  std::array<char, 256u> _new_project_name_buffer{};

}; // class launcher_module

} // namespace launcher

#endif // LAUNCHER_LAUNCHER_MODULE_HPP_
