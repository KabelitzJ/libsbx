// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/projects_module.hpp>

#include <chrono>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/user_data_directory.hpp>

namespace sbx::core {

projects_module::projects_module() {
  _load_recents();
}

auto projects_module::open(const std::filesystem::path& path) -> core::project& {
  const auto file = std::filesystem::is_directory(path) ? (path / core::project::file_name) : path;

  auto loaded = core::project::load(file);
  auto& active = core::engine::set_project(loaded);

  _touch_recent(active);

  return active;
}

auto projects_module::create(const std::filesystem::path& root, std::string name) -> core::project& {
  auto created = core::project::open_or_create(root, std::move(name));
  auto& active = core::engine::set_project(created);

  _touch_recent(active);

  return active;
}

auto projects_module::remove_recent(const std::filesystem::path& file) -> void {
  const auto canonical_file = std::filesystem::weakly_canonical(file);

  std::erase_if(_recents, [&](const auto& entry) {
    return std::filesystem::weakly_canonical(entry.file) == canonical_file;
  });

  _save_recents();
}

auto projects_module::_recents_file() const -> std::filesystem::path {
  return user_data_directory() / "recent_projects.yaml";
}

auto projects_module::_load_recents() -> void {
  const auto file = _recents_file();

  if (!std::filesystem::exists(file)) {
    return;
  }

  try {
    const auto root = YAML::LoadFile(file.string());
    const auto list = root["recent_projects"];

    if (!list) {
      return;
    }

    for (const auto& node : list) {
      if (!node["file"]) {
        continue;
      }

      auto entry = recent_project_entry{};

      entry.file = node["file"].as<std::string>();
      entry.name = node["name"] ? node["name"].as<std::string>() : std::string{"Untitled"};
      entry.last_opened = node["last_opened"] ? node["last_opened"].as<std::int64_t>() : std::int64_t{0};

      _recents.push_back(std::move(entry));
    }
  } catch (const std::exception& exception) {
    utility::logger<"core">::warn("Failed to read recent projects list at '{}': {}", file.string(), exception.what());
  }
}

auto projects_module::_save_recents() const -> void {
  const auto file = _recents_file();

  std::filesystem::create_directories(file.parent_path());

  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;
  emitter << YAML::Key << "recent_projects" << YAML::Value << YAML::BeginSeq;

  for (const auto& entry : _recents) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "file" << YAML::Value << entry.file.generic_string();
    emitter << YAML::Key << "name" << YAML::Value << entry.name;
    emitter << YAML::Key << "last_opened" << YAML::Value << entry.last_opened;
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;

  auto stream = std::ofstream{file};
  stream << emitter.c_str();
}

auto projects_module::_touch_recent(const core::project& project) -> void {
  const auto file = std::filesystem::weakly_canonical(project.project_file());

  std::erase_if(_recents, [&](const auto& entry) {
    return std::filesystem::weakly_canonical(entry.file) == file;
  });

  const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

  _recents.insert(_recents.begin(), recent_project_entry{file, project.name(), now});

  _save_recents();
}

} // namespace sbx::core
