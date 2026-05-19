// SPDX-License-Identifier: MIT
#include <libsbx/assets/meta_file.hpp>

#include <fstream>

#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

auto meta_path_for(const std::filesystem::path& source) -> std::filesystem::path {
  auto path = source;
  path += ".meta";

  return path;
}

auto read_meta_file(const std::filesystem::path& meta_path) -> std::optional<meta_data> {
  if (!std::filesystem::exists(meta_path)) {
    return std::nullopt;
  }

  try {
    const auto node = YAML::LoadFile(meta_path.string());

    if (!node || !node["uuid"] || !node["type"]) {
      utility::logger<"assets">::warn("Meta file '{}' missing 'uuid' or 'type'", meta_path.string());

      return std::nullopt;
    }

    auto data = meta_data{};

    data.id = node["uuid"].as<math::uuid>();
    data.type = node["type"].as<std::string>();

    if (node["import"]) {
      data.import_settings = node["import"];
    }

    return data;
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Failed to read meta file '{}': {}", meta_path.string(), exception.what());

    return std::nullopt;
  }
}

auto write_meta_file(const std::filesystem::path& meta_path, const meta_data& data) -> void {
  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;

  emitter << YAML::Key << "uuid" << YAML::Value << data.id;
  emitter << YAML::Key << "type" << YAML::Value << data.type;

  if (data.import_settings && data.import_settings.IsDefined() && !data.import_settings.IsNull()) {
    emitter << YAML::Key << "import" << YAML::Value << data.import_settings;
  }

  emitter << YAML::EndMap;

  auto stream = std::ofstream{meta_path};

  if (!stream) {
    throw utility::runtime_error{"Failed to open meta file '{}' for writing", meta_path.string()};
  }

  stream << emitter.c_str();

  stream.flush();
  stream.close();
}

} // namespace sbx::assets
