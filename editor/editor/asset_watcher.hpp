// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_ASSET_WATCHER_HPP_
#define EDITOR_ASSET_WATCHER_HPP_

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Notices files added, changed or deleted in the assets directory outside the editor, like Unity's asset refresh.
 *
 * New files go to the import window (or are imported directly when their type has no importer), changed sources of loaded assets are reimported, and deleted ones are dropped from the manifest.
 */
class asset_watcher {

public:

  /**
   * @brief Rescans at most once per interval; call every frame.
   *
   * @param state The editor state.
   */
  auto update(editor_state& state) -> void;

private:

  using snapshot = std::unordered_map<std::string, std::filesystem::file_time_type>;

  [[nodiscard]] static auto _scan() -> snapshot;

  inline static constexpr auto _interval = std::chrono::seconds{1};

  snapshot _files{};
  std::chrono::steady_clock::time_point _last_scan{};
  bool _has_snapshot{false};

}; // class asset_watcher

} // namespace editor

#endif // EDITOR_ASSET_WATCHER_HPP_
