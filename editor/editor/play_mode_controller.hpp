// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PLAY_MODE_CONTROLLER_HPP_
#define EDITOR_PLAY_MODE_CONTROLLER_HPP_

#include <filesystem>

#include <libsbx/utility/noncopyable.hpp>

namespace editor {

/** @brief Where the Play/Pause/Stop workflow is. */
enum class play_state {
  edit,
  playing,
  paused,
}; // enum class play_state

/**
 * @brief Drives Play/Pause/Stop: Play snapshots the scene to a scratch file under `.sbx/` and starts simulating; Stop destroys script instances, stops simulating and reloads the snapshot in place.
 *
 * Owns the play state, since scenes_module only exposes a generic simulating flag.
 */
class play_mode_controller final : public sbx::utility::noncopyable {

public:

  play_mode_controller();

  [[nodiscard]] auto state() const noexcept -> play_state {
    return _state;
  }

  /** @brief Enters play mode; no-op unless in Edit mode. */
  auto enter_play_mode() -> void;

  /** @brief Returns to Edit mode; no-op unless playing or paused. */
  auto exit_play_mode() -> void;

  /**
   * @brief Pauses or resumes; no-op unless playing or paused.
   *
   * @param value Whether to pause.
   */
  auto set_paused(bool value) -> void;

  auto toggle_pause() -> void;

private:

  [[nodiscard]] auto _snapshot_path() const -> std::filesystem::path;

  play_state _state{play_state::edit};

}; // class play_mode_controller

} // namespace editor

#endif // EDITOR_PLAY_MODE_CONTROLLER_HPP_
