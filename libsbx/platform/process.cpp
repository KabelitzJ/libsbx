// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/platform/process.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

#if defined(SBX_PLATFORM_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace sbx::platform {

#if defined(SBX_PLATFORM_WIN32)

auto spawn(const std::filesystem::path& executable, const spawn_options& options) -> void {
  auto command_line = std::wstring{L"\""} + executable.wstring() + L"\"";

  for (const auto& argument : options.arguments) {
    command_line += L" \"" + std::filesystem::path{argument}.wstring() + L"\"";
  }

  auto startup_info = STARTUPINFOW{};
  startup_info.cb = sizeof(startup_info);

  auto process_info = PROCESS_INFORMATION{};

  const auto succeeded = ::CreateProcessW(executable.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info, &process_info);

  if (!succeeded) {
    utility::logger<"platform">::error("Failed to launch '{}'", executable.string());
    return;
  }

  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(process_info.hThread);
}

#else

auto spawn(const std::filesystem::path& executable, const spawn_options& options) -> void {
  const auto pid = ::fork();

  if (pid < 0) {
    utility::logger<"platform">::error("fork() failed while launching '{}'", executable.string());
    return;
  }

  if (pid == 0) {
    // Child: replace this process image entirely. execv only returns on failure.
    auto argv = std::vector<char*>{};
    argv.reserve(options.arguments.size() + 2u);

    auto executable_string = executable.string();
    argv.push_back(executable_string.data());

    auto argument_strings = std::vector<std::string>{options.arguments.begin(), options.arguments.end()};

    for (auto& argument : argument_strings) {
      argv.push_back(argument.data());
    }

    argv.push_back(nullptr);

    ::execv(executable.c_str(), argv.data());
    ::_exit(127);
  }
}

#endif

} // namespace sbx::platform
