// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <fstream>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/assets/shader_graph_codegen.hpp>

namespace sbx::assets {

auto asset_cooker::parse_shader_graph_file(const std::filesystem::path& source) -> std::optional<shader_graph_description> {
  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(source.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse shader_graph '{}' ({})", source.generic_string(), exception.what());
    return std::nullopt;
  }

  auto description = shader_graph_description{};

  if (root["name"]) description.name = root["name"].as<std::string>();

  if (const auto nodes = root["nodes"]) {
    description.nodes.reserve(nodes.size());

    for (const auto node_yaml : nodes) {
      auto node = shader_graph_node_description{};

      if (node_yaml["id"]) node.id = node_yaml["id"].as<std::uint32_t>();
      if (node_yaml["type"]) node.type = shader_node_type_from_string(node_yaml["type"].as<std::string>());
      if (node_yaml["position"]) node.editor_position = node_yaml["position"].as<math::vector2>();
      if (node_yaml["name"]) node.name = node_yaml["name"].as<std::string>();
      if (node_yaml["exposed"]) node.exposed = node_yaml["exposed"].as<bool>();

      switch (node.type) {
        case shader_node_type::constant_float:
          node.value = node_yaml["value"] ? node_yaml["value"].as<std::float_t>() : 0.0f;
          break;
        case shader_node_type::constant_vector3:
          node.value = node_yaml["value"] ? node_yaml["value"].as<math::vector3>() : math::vector3{};
          break;
        case shader_node_type::constant_color:
          node.value = node_yaml["value"] ? node_yaml["value"].as<math::color>() : math::color{};
          break;
        case shader_node_type::texture_sample:
          node.value = node_yaml["texture"] ? node_yaml["texture"].as<std::string>() : std::string{};
          break;
        case shader_node_type::swizzle:
          node.value = node_yaml["pattern"] ? node_yaml["pattern"].as<std::string>() : std::string{"rgba"};
          break;
        default:
          break; // monostate -- math/input/output nodes carry no payload
      }

      description.nodes.push_back(node);
    }
  }

  if (const auto edges = root["edges"]) {
    description.edges.reserve(edges.size());

    for (const auto edge_yaml : edges) {
      auto edge = shader_graph_edge{};

      if (edge_yaml["from_node"]) edge.from_node = edge_yaml["from_node"].as<std::uint32_t>();
      if (edge_yaml["from_pin"]) edge.from_pin = edge_yaml["from_pin"].as<std::uint32_t>();
      if (edge_yaml["to_node"]) edge.to_node = edge_yaml["to_node"].as<std::uint32_t>();
      if (edge_yaml["to_pin"]) edge.to_pin = edge_yaml["to_pin"].as<std::uint32_t>();

      description.edges.push_back(edge);
    }
  }

  return description;
}

auto asset_cooker::cook_shader_graph(const math::uuid& id, std::uint64_t generation, const shader_graph::create_info& create_info) -> bool {
  const auto graph_name = shader_graph_generated_name(id, generation);
  const auto result = generate_shader_graph_source(graph_name, create_info);

  if (!result) {
    utility::logger<"assets">::warn("Could not cook shader_graph {} ({})", id, result.error());
    return false;
  }

  // Lives inside the engine's own shaders tree, not the usual cooked_path()/library-directory
  // convention every other cooker uses -- shader_compiler resolves a compiled file's #includes
  // relative to the nearest "shaders" ancestor directory (see shader_compiler.cpp's _shaders_root),
  // so the generated file has to actually sit inside that tree (alongside geometry_common.slang
  // etc.) for its own #include <geometry_common.slang> to resolve at all.
  //
  // Named after graph_name (not a bare uuid) since shader_compiler loads a module using the file's
  // stem as its Slang module name (path.stem() in shader_compiler.cpp) -- a purely-numeric stem
  // risks not being a valid module identifier, so this matches the generated struct's own name.
  const auto directory = filesystem::engine_data_directory() / "shaders" / "generated";
  const auto path = directory / fmt::format("{}.slang", graph_name);

  auto error = std::error_code{};
  std::filesystem::create_directories(directory, error);

  // Every earlier generation's file for this same graph is unreachable the moment this one's
  // written (shader_graph_generated_path always asks for the CURRENT generation) -- clean it up so
  // a long editing session doesn't leave one stale .slang file behind per edit. Only the file on
  // disk; the shader_cache/pipeline_cache entries that earlier generation already compiled into
  // still exist in memory (see shader_graph_generated_name's doc comment for why that's left alone).
  const auto stale_prefix = fmt::format("shader_graph_{}_", id.value());

  if (std::filesystem::exists(directory, error) && !error) {
    for (const auto& entry : std::filesystem::directory_iterator{directory, error}) {
      if (entry.path() == path) {
        continue;
      }

      if (const auto filename = entry.path().filename().string(); filename.starts_with(stale_prefix) && entry.path().extension() == ".slang") {
        auto remove_error = std::error_code{};
        std::filesystem::remove(entry.path(), remove_error);
      }
    }
  }

  auto out = std::ofstream{path, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Could not write generated shader for shader_graph {} to '{}'", id, path.generic_string());
    return false;
  }

  out << *result;

  utility::logger<"assets">::debug("Cooked shader_graph {} -> '{}'", id, path.generic_string());

  return true;
}

} // namespace sbx::assets
