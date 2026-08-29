// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PLAY_MODE_CONTROLLER_HPP_
#define EDITOR_PLAY_MODE_CONTROLLER_HPP_

#include <filesystem>

#include <libsbx/utility/noncopyable.hpp>

namespace editor {

/** @brief Where the editor's play/pause/stop workflow currently is. */
enum class play_state {
  edit,
  playing,
  paused,
}; // enum class play_state

/**
 * @brief Drives the editor's Play/Pause/Stop workflow. On Play, snapshots the active scene to a
 * scratch file under the project's `.sbx/` directory and turns scene simulation on; on Stop,
 * tears down any live script instances, turns simulation back off, and reloads the snapshot over
 * the same scene in place — restoring exactly the pre-play state without ever holding a second
 * copy of the scene in memory.
 *
 * This class owns "play state" because scenes::scenes_module deliberately doesn't — it's linked
 * into runtime/launcher too, neither of which has any notion of editor/play/edit. scenes_module
 * only exposes a generic is_simulating()/set_simulating() gate; this is what drives it.
 */
class play_mode_controller final : public sbx::utility::noncopyable {

public:

  play_mode_controller();

  [[nodiscard]] auto state() const noexcept -> play_state {
    return _state;
  }

  /** @brief No-op unless currently play_state::edit. */
  auto enter_play_mode() -> void;

  /** @brief No-op unless currently playing or paused. */
  auto exit_play_mode() -> void;

  /** @brief No-op unless currently playing or paused. */
  auto set_paused(bool value) -> void;

  auto toggle_pause() -> void;

private:

  [[nodiscard]] auto _snapshot_path() const -> std::filesystem::path;

  play_state _state{play_state::edit};

}; // class play_mode_controller

} // namespace editor

#endif // EDITOR_PLAY_MODE_CONTROLLER_HPP_
