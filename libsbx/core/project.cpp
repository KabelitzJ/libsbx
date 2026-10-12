// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/project.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
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

  if (const auto scenes_node = node["scenes"]) {
    for (const auto& entry : scenes_node) {
      result._scene_list.push_back(scene_list_entry{entry["path"].as<std::string>(), entry["enabled"].as<bool>(true)});
    }
  } else if (node["startup_scene"]) {
    // v4 and older: the single startup scene becomes the list's first entry.
    result._scene_list.push_back(scene_list_entry{node["startup_scene"].as<std::string>(), true});
  }

  if (const auto layers_node = node["layers"]) {
    for (auto index = std::size_t{0u}; index < layer_count && index < layers_node.size(); ++index) {
      result._layers[index] = layers_node[index].as<std::string>();
    }
  }

  if (const auto matrix_node = node["layer_collision_matrix"]) {
    for (auto index = std::size_t{0u}; index < layer_count && index < matrix_node.size(); ++index) {
      result._layer_collision_matrix[index] = matrix_node[index].as<std::uint32_t>();
    }
  }

  if (const auto physics_node = node["physics"]) {
    if (physics_node["gravity"]) {
      result._gravity = physics_node["gravity"].as<math::vector3>();
    }

    if (physics_node["velocity_iterations"]) {
      result._velocity_iterations = physics_node["velocity_iterations"].as<std::uint32_t>();
    }
  }

  if (const auto time_node = node["time"]; time_node && time_node["fixed_timestep"]) {
    result._fixed_timestep = time_node["fixed_timestep"].as<std::float_t>();
  }

  if (const auto audio_node = node["audio"]) {
    result._max_voices = audio_node["max_voices"].as<std::uint32_t>(result._max_voices);

    if (const auto groups_node = audio_node["mixer_groups"]) {
      auto groups = std::vector<mixer_group>{};

      for (const auto& group_node : groups_node) {
        groups.push_back(mixer_group{
          group_node["id"].as<math::uuid>(),
          group_node["name"].as<std::string>(""),
          group_node["parent"].as<math::uuid>(math::uuid::nil()),
          group_node["volume"].as<std::float_t>(1.0f),
          group_node["muted"].as<bool>(false)
        });
      }

      result.set_mixer_groups(std::move(groups));
    }
  }

  return result;
}

auto project::scene_count_in_build() const -> std::size_t {
  return static_cast<std::size_t>(std::ranges::count_if(_scene_list, &scene_list_entry::enabled));
}

auto project::scene_at(std::size_t build_index) const -> std::optional<std::filesystem::path> {
  for (const auto& entry : _scene_list | std::views::filter(&scene_list_entry::enabled)) {
    if (build_index-- == 0u) {
      return entry.path;
    }
  }

  return std::nullopt;
}

auto project::build_index_of(const std::filesystem::path& path) const -> std::optional<std::size_t> {
  const auto wanted = path.lexically_normal().generic_string();

  auto index = std::size_t{0u};

  for (const auto& entry : _scene_list | std::views::filter(&scene_list_entry::enabled)) {
    if (entry.path.lexically_normal().generic_string() == wanted) {
      return index;
    }

    ++index;
  }

  return std::nullopt;
}

auto project::resolve_scene(std::string_view name_or_path) const -> std::optional<std::filesystem::path> {
  const auto lowercase = [](std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
  };

  const auto is_path = name_or_path.find('/') != std::string_view::npos || std::filesystem::path{name_or_path}.extension() == ".scene";

  if (is_path) {
    auto path = std::filesystem::path{name_or_path};

    if (path.extension() != ".scene") {
      path += ".scene";
    }

    if (build_index_of(path)) {
      return path.lexically_normal();
    }

    return std::nullopt;
  }

  const auto wanted = lowercase(std::string{name_or_path});

  for (const auto& entry : _scene_list | std::views::filter(&scene_list_entry::enabled)) {
    if (lowercase(entry.path.stem().string()) == wanted) {
      return entry.path;
    }
  }

  return std::nullopt;
}

auto project::set_mixer_groups(std::vector<mixer_group> groups) -> void {
  const auto is_master = [](const mixer_group& group) { return group.id == master_mixer_group_id; };

  if (const auto master = std::ranges::find_if(groups, is_master); master == groups.end()) {
    groups.insert(groups.begin(), mixer_group{master_mixer_group_id, "Master"});
  } else if (master != groups.begin()) {
    std::rotate(groups.begin(), master, master + 1);
  }

  groups.front().parent = math::uuid::nil();

  // Reparents any group whose chain doesn't reach Master within groups.size() steps: a missing parent or a cycle.
  for (auto& group : groups | std::views::drop(1u)) {
    auto current = group.parent;

    for (auto steps = std::size_t{0u}; steps < groups.size() && current != master_mixer_group_id; ++steps) {
      const auto parent = std::ranges::find_if(groups, [&](const mixer_group& other) { return other.id == current; });
      current = parent != groups.end() ? parent->parent : math::uuid::nil();
    }

    if (current != master_mixer_group_id) {
      group.parent = master_mixer_group_id;
    }
  }

  _mixer_groups = std::move(groups);
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

  emitter << YAML::Key << "scenes" << YAML::Value << YAML::BeginSeq;
  for (const auto& entry : _scene_list) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "path" << YAML::Value << entry.path.generic_string();
    emitter << YAML::Key << "enabled" << YAML::Value << entry.enabled;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
  for (const auto& layer_name : _layers) {
    emitter << layer_name;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "layer_collision_matrix" << YAML::Value << YAML::BeginSeq;
  for (const auto row : _layer_collision_matrix) {
    emitter << row;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "physics" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "gravity" << YAML::Value << YAML::Node{_gravity};
  emitter << YAML::Key << "velocity_iterations" << YAML::Value << _velocity_iterations;
  emitter << YAML::EndMap;

  emitter << YAML::Key << "time" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "fixed_timestep" << YAML::Value << _fixed_timestep;
  emitter << YAML::EndMap;

  emitter << YAML::Key << "audio" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "max_voices" << YAML::Value << _max_voices;
  emitter << YAML::Key << "mixer_groups" << YAML::Value << YAML::BeginSeq;
  for (const auto& group : _mixer_groups) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "id" << YAML::Value << YAML::Node{group.id};
    emitter << YAML::Key << "name" << YAML::Value << group.name;
    emitter << YAML::Key << "parent" << YAML::Value << YAML::Node{group.parent};
    emitter << YAML::Key << "volume" << YAML::Value << group.volume;
    emitter << YAML::Key << "muted" << YAML::Value << group.muted;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;

  emitter << YAML::EndMap;
  emitter << YAML::EndMap;

  std::filesystem::create_directories(file.parent_path());

  auto stream = std::ofstream{file};
  stream << emitter.c_str();
}

} // namespace sbx::core
