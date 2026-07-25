// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/project.hpp>

#include <fstream>
#include <stdexcept>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

namespace sbx::core {

auto project::project_file() const -> std::filesystem::path {
  return _root / (std::string{_name} + std::string{file_extension});
}

auto project::load(const std::filesystem::path& file) -> project {
  if (!std::filesystem::exists(file)) {
    throw std::runtime_error{fmt::format("Project file '{}' does not exist", file.string())};
  }

  const auto root = YAML::LoadFile(file.string());
  const auto node = root["project"] ? root["project"] : root;

  auto result = project{};

  result._root = file.parent_path();
  result._name = node["name"] ? node["name"].as<std::string>() : std::string{"Untitled"};

  if (node["assets"]) {
    result._assets = node["assets"].as<std::string>();
  }

  if (node["library"]) {
    result._library = node["library"].as<std::string>();
  }

  return result;
}

auto project::open_or_create(const std::filesystem::path& root, std::string name) -> project {
  const auto file = root / (std::string{name} + std::string{file_extension});

  if (std::filesystem::exists(file)) {
    return load(file);
  }

  auto created = project{root, std::move(name)};

  std::filesystem::create_directories(created.assets_directory());
  std::filesystem::create_directories(created.library_directory());

  created.save();

  return created;
}

auto project::save() const -> void {
  save(project_file());
}

auto project::save(const std::filesystem::path& file) const -> void {
  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;
  emitter << YAML::Key << "project" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "name" << YAML::Value << _name;
  emitter << YAML::Key << "assets" << YAML::Value << _assets.generic_string();
  emitter << YAML::Key << "library" << YAML::Value << _library.generic_string();
  emitter << YAML::EndMap;
  emitter << YAML::EndMap;

  std::filesystem::create_directories(file.parent_path());

  auto stream = std::ofstream{file};
  stream << emitter.c_str();
}

} // namespace sbx::core
