// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/project.hpp>

#include <fstream>
#include <stdexcept>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

namespace sbx::core {

auto project::load(const std::filesystem::path& file) -> project {
  if (!std::filesystem::exists(file)) {
    throw std::runtime_error{fmt::format("Project file '{}' does not exist", file.string())};
  }

  const auto root = YAML::LoadFile(file.string());
  const auto node = root["project"] ? root["project"] : root;

  const auto format_version = node["format_version"] ? node["format_version"].as<std::uint32_t>() : std::uint32_t{1u};

  if (format_version > current_format_version) {
    throw std::runtime_error{fmt::format(
      "Project file '{}' was saved by a newer version of the engine (format_version {}, this engine supports up to {})",
      file.string(), format_version, current_format_version
    )};
  }

  auto result = project{};

  result._root = file.parent_path();
  result._name = node["name"] ? node["name"].as<std::string>() : std::string{"Untitled"};

  if (node["assets"]) {
    result._assets = node["assets"].as<std::string>();
  }

  if (node["library"]) {
    result._library = node["library"].as<std::string>();
  }

  if (node["logs"]) {
    result._logs = node["logs"].as<std::string>();
  }

  if (node["startup_scene"]) {
    result._startup_scene = std::filesystem::path{node["startup_scene"].as<std::string>()};
  }

  return result;
}

auto project::open_or_create(const std::filesystem::path& root, std::string name) -> project {
  const auto file = root / file_name;

  if (std::filesystem::exists(file)) {
    return load(file);
  }

  auto created = project{root, std::move(name)};

  std::filesystem::create_directories(created.assets_directory());
  std::filesystem::create_directories(created.logs_directory());
  std::filesystem::create_directories(created.library_directory());

  const auto gitignore = root / ".gitignore";

  if (!std::filesystem::exists(gitignore)) {
    auto stream = std::ofstream{gitignore};
    stream << ".sbx/\n";
    // scripting::script_compiler regenerates this on every engine start purely so an IDE can
    // resolve Sbx.Core for autocomplete — it's machine-specific (an absolute HintPath) and never
    // built by the engine, so it (and any bin/obj an IDE might create from it) don't belong in
    // source control.
    stream << "assets/Game.csproj\n";
    stream << "assets/bin/\n";
    stream << "assets/obj/\n";
  }

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
  emitter << YAML::Key << "format_version" << YAML::Value << current_format_version;
  emitter << YAML::Key << "name" << YAML::Value << _name;
  emitter << YAML::Key << "assets" << YAML::Value << _assets.generic_string();
  emitter << YAML::Key << "library" << YAML::Value << _library.generic_string();
  emitter << YAML::Key << "logs" << YAML::Value << _logs.generic_string();

  if (_startup_scene) {
    emitter << YAML::Key << "startup_scene" << YAML::Value << _startup_scene->generic_string();
  }

  emitter << YAML::EndMap;
  emitter << YAML::EndMap;

  std::filesystem::create_directories(file.parent_path());

  auto stream = std::ofstream{file};
  stream << emitter.c_str();
}

} // namespace sbx::core
