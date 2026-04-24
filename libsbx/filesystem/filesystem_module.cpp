// SPDX-License-Identifier: MIT
#include <libsbx/filesystem/filesystem_module.hpp>

#include <cstdlib>
#include <filesystem>

#include <libsbx/utility/logger.hpp>

#include <libsbx/filesystem/native_filesystem.hpp>
#include <libsbx/filesystem/data_directory.hpp>

namespace sbx::filesystem {

filesystem_module::filesystem_module()
: _filesystem{std::make_unique<virtual_filesystem>()} {
  auto engine_directory = std::filesystem::path{};

  if (const auto* env = std::getenv("SBX_ENGINE_DATA_DIR"); env && *env) {
    engine_directory = env;
  } else {
    engine_directory = filesystem::data_directory;
  }

  if (!std::filesystem::exists(engine_directory) || !std::filesystem::is_directory(engine_directory)) {
    utility::logger<"filesystem">::warn("Engine data directory '{}' does not exist; the 'engine://' mount will not be registered", engine_directory.string());
    return;
  }

  const auto mount = _filesystem->create_filesystem<native_filesystem>(alias{"engine://"}, engine_directory.string());

  if (!mount) {
    utility::logger<"filesystem">::warn("Failed to register 'engine://' mount at '{}'", engine_directory.string());
    return;
  }

  utility::logger<"filesystem">::debug("Registered 'engine://' mount at '{}'", engine_directory.string());
}

filesystem_module::~filesystem_module() {

}

auto filesystem_module::update() -> void {
  SBX_PROFILE_SCOPE("filesystem_module::update");
}

} // namespace sbx::filesystem
