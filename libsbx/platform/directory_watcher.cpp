// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/platform/directory_watcher.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>
#include <system_error>

#include <libsbx/utility/logger.hpp>

#if defined(SBX_PLATFORM_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/inotify.h>
  #include <unistd.h>
#endif

namespace sbx::platform {

#if defined(SBX_PLATFORM_WIN32)

directory_watcher::directory_watcher(const std::filesystem::path& root) {
  const auto handle = ::FindFirstChangeNotificationW(root.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);

  if (handle == INVALID_HANDLE_VALUE) {
    utility::logger<"platform">::warn("Could not watch '{}' for changes; falling back to rescanning it every time", root.string());
    return;
  }

  _handle = reinterpret_cast<std::intptr_t>(handle);
}

directory_watcher::~directory_watcher() {
  if (_handle != -1) {
    ::FindCloseChangeNotification(reinterpret_cast<HANDLE>(_handle));
  }
}

auto directory_watcher::poll() -> bool {
  if (_handle == -1) {
    return true;
  }

  const auto handle = reinterpret_cast<HANDLE>(_handle);

  if (::WaitForSingleObject(handle, 0u) != WAIT_OBJECT_0) {
    return false;
  }

  // Re-arms the notification; changes between the signal and here are reported by the next signal.
  ::FindNextChangeNotification(handle);

  return true;
}

auto directory_watcher::_watch_tree(const std::filesystem::path& /*directory*/) -> void {
  // FindFirstChangeNotificationW already watches the whole subtree.
}

#else

inline constexpr auto watch_mask = std::uint32_t{IN_CREATE | IN_DELETE | IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF};

directory_watcher::directory_watcher(const std::filesystem::path& root) {
  const auto descriptor = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

  if (descriptor < 0) {
    utility::logger<"platform">::warn("Could not watch '{}' for changes ({}); falling back to rescanning it every time", root.string(), std::strerror(errno));
    return;
  }

  _handle = descriptor;

  _watch_tree(root);
}

directory_watcher::~directory_watcher() {
  if (_handle != -1) {
    ::close(static_cast<int>(_handle));
  }
}

auto directory_watcher::poll() -> bool {
  if (_handle == -1) {
    return true;
  }

  auto changed = false;

  alignas(inotify_event) auto buffer = std::array<char, 16384u>{};

  while (true) {
    const auto length = ::read(static_cast<int>(_handle), buffer.data(), buffer.size());

    // EAGAIN: nothing left to read.
    if (length <= 0) {
      break;
    }

    for (auto offset = std::size_t{0u}; offset + sizeof(inotify_event) <= static_cast<std::size_t>(length);) {
      auto event = inotify_event{};
      std::memcpy(&event, buffer.data() + offset, sizeof(inotify_event));

      const auto name = std::string_view{buffer.data() + offset + sizeof(inotify_event), event.len};

      changed = true;

      if ((event.mask & IN_ISDIR) != 0u && (event.mask & (IN_CREATE | IN_MOVED_TO)) != 0u) {
        if (const auto parent = _directories.find(event.wd); parent != _directories.end()) {
          // The name is NUL-padded to event.len.
          _watch_tree(parent->second / std::string{name.substr(0u, name.find('\0'))});
        }
      }

      if ((event.mask & IN_IGNORED) != 0u) {
        _directories.erase(event.wd);
      }

      offset += sizeof(inotify_event) + event.len;
    }
  }

  return changed;
}

auto directory_watcher::_watch_tree(const std::filesystem::path& directory) -> void {
  if (_handle == -1) {
    return;
  }

  const auto add = [this](const std::filesystem::path& path) {
    const auto watch = ::inotify_add_watch(static_cast<int>(_handle), path.c_str(), watch_mask);

    if (watch < 0) {
      // Usually the per-user watch limit; without every directory watched, changes could go unnoticed.
      utility::logger<"platform">::warn("Could not watch '{}' ({}); falling back to rescanning every time", path.string(), std::strerror(errno));
      ::close(static_cast<int>(_handle));
      _handle = -1;
      _directories.clear();
      return false;
    }

    _directories[watch] = path;
    return true;
  };

  if (!add(directory)) {
    return;
  }

  auto error = std::error_code{};
  auto iterator = std::filesystem::recursive_directory_iterator{directory, std::filesystem::directory_options::skip_permission_denied, error};

  for (const auto end = std::filesystem::recursive_directory_iterator{}; !error && iterator != end; iterator.increment(error)) {
    auto entry_error = std::error_code{};

    if (iterator->is_directory(entry_error) && !iterator->is_symlink(entry_error) && !add(iterator->path())) {
      return;
    }
  }
}

#endif

} // namespace sbx::platform
