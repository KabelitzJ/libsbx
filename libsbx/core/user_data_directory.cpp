// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/user_data_directory.hpp>

#include <cstdlib>
#include <stdexcept>

#include <libsbx/utility/target.hpp>

#if defined(SBX_PLATFORM_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <shlobj.h>
#endif

namespace sbx::core {

auto user_data_directory() -> std::filesystem::path {
#if defined(SBX_PLATFORM_WIN32)
  auto* path = static_cast<wchar_t*>(nullptr);

  if (::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path) != S_OK) {
    throw std::runtime_error{"SHGetKnownFolderPath(FOLDERID_RoamingAppData) failed"};
  }

  auto result = std::filesystem::path{path} / "libsbx";
  ::CoTaskMemFree(path);

  return result;
#elif defined(SBX_PLATFORM_APPLE)
  if (auto* home = std::getenv("HOME")) {
    return std::filesystem::path{home} / "Library" / "Application Support" / "libsbx";
  }

  throw std::runtime_error{"HOME environment variable not set"};
#else
  if (auto* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
    return std::filesystem::path{xdg_config_home} / "libsbx";
  }

  if (auto* home = std::getenv("HOME")) {
    return std::filesystem::path{home} / ".config" / "libsbx";
  }

  throw std::runtime_error{"Neither XDG_CONFIG_HOME nor HOME environment variable is set"};
#endif
}

auto user_home_directory() -> std::filesystem::path {
#if defined(SBX_PLATFORM_WIN32)
  if (auto* profile = std::getenv("USERPROFILE")) {
    return std::filesystem::path{profile};
  }

  throw std::runtime_error{"USERPROFILE environment variable not set"};
#else
  if (auto* home = std::getenv("HOME")) {
    return std::filesystem::path{home};
  }

  throw std::runtime_error{"HOME environment variable not set"};
#endif
}

} // namespace sbx::core
