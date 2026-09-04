// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/play_mode_controller.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

#include <libsbx/utility/logger.hpp>

namespace editor {

play_mode_controller::play_mode_controller() {
  sbx::core::engine::get_module<sbx::scenes::scenes_module>().set_simulating(false);
}

auto play_mode_controller::_snapshot_path() const -> std::filesystem::path {
  return sbx::core::engine::project().root() / ".sbx" / "editor" / "play_snapshot.yaml";
}

auto play_mode_controller::enter_play_mode() -> void {
  if (_state != play_state::edit) {
    return;
  }

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  if (!scripting_module.last_compile_succeeded()) {
    sbx::utility::logger<"scripting">::error("Cannot enter Play mode — scripts have compile errors. See the Console for details.");

    return;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  sbx::scenes::scene_serializer::save(scenes_module.active_scene(), _snapshot_path());

  scenes_module.set_simulating(true);

  scripting_module.instantiate_scene_scripts(scenes_module.active_scene());

  _state = play_state::playing;
}

auto play_mode_controller::exit_play_mode() -> void {
  if (_state == play_state::edit) {
    return;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  scripting_module.run_on_destroy(scenes_module.active_scene());

  scenes_module.set_simulating(false);

  sbx::scenes::scene_serializer::load(scenes_module.active_scene(), _snapshot_path());

  std::filesystem::remove(_snapshot_path());

  _state = play_state::edit;
}

auto play_mode_controller::set_paused(bool value) -> void {
  if (_state == play_state::edit) {
    return;
  }

  sbx::core::engine::get_module<sbx::scenes::scenes_module>().set_simulating(!value);

  _state = value ? play_state::paused : play_state::playing;
}

auto play_mode_controller::toggle_pause() -> void {
  set_paused(_state == play_state::playing);
}

} // namespace editor
