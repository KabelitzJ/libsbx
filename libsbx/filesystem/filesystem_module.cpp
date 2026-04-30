// SPDX-License-Identifier: MIT
#include <libsbx/filesystem/filesystem_module.hpp>

#include <cstdlib>
#include <filesystem>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/profiler.hpp>

#if defined(SBX_WINDOWS)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#elif defined(SBX_APPLE)
  #include <cstdint>
  #include <mach-o/dyld.h>
#endif

#include <libsbx/filesystem/native_filesystem.hpp>

namespace sbx::filesystem {

auto _executable_directory() -> std::filesystem::path {
#if defined(_WIN32)
  auto buffer = std::vector<wchar_t>(MAX_PATH);

  for (;;) {
    const auto size = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<::DWORD>(buffer.size()));

    if (size == 0) {
      throw std::runtime_error{"GetModuleFileNameW failed"};
    }

    if (size < buffer.size()) {
      return std::filesystem::path{std::wstring{buffer.data(), size}}.parent_path();
    }

    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  auto buffer = std::vector<char>(1024);
  auto size = static_cast<std::uint32_t>(buffer.size());

  if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
    buffer.resize(size);
    if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
      throw std::runtime_error{"_NSGetExecutablePath failed"};
    }
  }

  return std::filesystem::canonical(std::filesystem::path{buffer.data()}).parent_path();
#else
  return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

auto _has_marker(const std::filesystem::path& candidate) -> bool {
  auto ec = std::error_code{};
  return std::filesystem::exists(candidate / "shaders" / "manifest.txt", ec);
}

auto _data_directory() -> std::filesystem::path {
  static const auto root = []() -> std::filesystem::path {
    if (auto* env = std::getenv("SBX_DATA_DIR")) return env;

    const auto exe_dir = _executable_directory();

    if (auto candidate = exe_dir / ".." / "data"; _has_marker(candidate)) {
      return std::filesystem::canonical(candidate);
    }

    if (auto candidate = exe_dir / ".." / "share" / "libsbx"; _has_marker(candidate)) {
      return std::filesystem::canonical(candidate);
    }

    throw std::runtime_error{"engine data directory not found"};
  }();

  return root;
}

filesystem_module::filesystem_module()
: _filesystem{std::make_unique<virtual_filesystem>()} {
  auto data_directory = std::filesystem::path{};

  try {
    data_directory = _data_directory();
  } catch (const std::exception& exception) {
    utility::logger<"filesystem">::warn("Engine data directory not found: {}; the 'engine://' mount will not be registered", exception.what());
    return;
  }

  const auto mount = _filesystem->create_filesystem<native_filesystem>(alias{"engine://"}, data_directory.string());

  if (!mount) {
    utility::logger<"filesystem">::warn("Failed to register 'engine://' mount at '{}'", data_directory.string());
    return;
  }

  utility::logger<"filesystem">::debug("Registered 'engine://' mount at '{}'", data_directory.string());
}

filesystem_module::~filesystem_module() {

}

auto filesystem_module::update() -> void {
  SBX_PROFILE_SCOPE("filesystem_module::update");
}

} // namespace sbx::filesystem
