// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_DIRECTORY_WATCHER_HPP_
#define LIBSBX_PLATFORM_DIRECTORY_WATCHER_HPP_

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::platform {

/**
 * @brief Reports whether anything under a directory tree changed, without scanning it: inotify on Linux, a change notification on Windows.
 *
 * It says that something changed, not what; the caller rescans to find out.
 * If the OS facility can't be set up (e.g. the inotify watch limit is reached), poll() always reports a change, so callers fall back to scanning every time.
 */
class directory_watcher : public utility::noncopyable {

public:

  /**
   * @brief Starts watching @p root and every directory below it.
   *
   * @param root The directory tree to watch.
   */
  explicit directory_watcher(const std::filesystem::path& root);

  ~directory_watcher();

  /**
   * @brief Drains pending notifications without blocking.
   *
   * @return True if a file or directory under the root was created, changed, moved or deleted since the last call.
   */
  [[nodiscard]] auto poll() -> bool;

private:

  /**
   * @brief Adds a watch for @p directory and every directory below it (inotify only watches one directory per watch).
   *
   * @param directory The directory to watch.
   */
  auto _watch_tree(const std::filesystem::path& directory) -> void;

  // The inotify descriptor, or the Windows change-notification HANDLE; -1 when unavailable.
  std::intptr_t _handle{-1};

  // inotify watch descriptor -> watched directory, to resolve where a newly created subdirectory lives.
  std::unordered_map<int, std::filesystem::path> _directories{};

}; // class directory_watcher

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_DIRECTORY_WATCHER_HPP_
