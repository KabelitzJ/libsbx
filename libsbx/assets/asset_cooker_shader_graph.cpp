// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <numeric>
#include <optional>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/assets/shader_graph_codegen.hpp>

namespace sbx::assets {

// Graphs saved before blackboard properties exposed constant nodes directly and made every Sample Texture a material texture.
// Each becomes a property in the slot it had, so materials using the graph keep their values.
static auto migrate_to_properties(shader_graph_description& description) -> void {
  auto next_node_id = std::uint32_t{0u};

  for (const auto& node : description.nodes) {
    next_node_id = std::max(next_node_id, node.id + 1u);
  }

  auto order = std::vector<std::size_t>(description.nodes.size());
  std::iota(order.begin(), order.end(), std::size_t{0u});
  std::ranges::sort(order, {}, [&description](std::size_t index) { return description.nodes[index].id; });

  auto properties = std::vector<shader_graph_property>{};
  auto float_slot = std::uint32_t{0u};
  auto texture_slot = std::uint32_t{0u};
  auto new_nodes = std::vector<shader_graph_node_description>{};

  const auto add_property = [&](const std::string& name, const std::string& fallback_name, shader_graph_parameter_type type, std::uint32_t slot) -> shader_graph_property& {
    auto property = shader_graph_property{};
    property.id = shader_graph_next_property_id(properties);
    property.name = name.empty() ? fmt::format("{} {}", fallback_name, property.id) : name;
    property.reference = shader_graph_make_reference(property.name, properties);
    property.type = type;
    property.slot = slot;

    return properties.emplace_back(std::move(property));
  };

  for (const auto index : order) {
    auto& node = description.nodes[index];

    if (node.type == shader_node_type::texture_sample) {
      auto& property = add_property(node.name, "Texture", shader_graph_parameter_type::texture_value, texture_slot++);

      auto texture_path = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
      description.properties.push_back(shader_graph_property_description{property, std::move(texture_path)});
      node.value = std::monostate{};

      auto property_node = shader_graph_node_description{};
      property_node.id = next_node_id++;
      property_node.type = shader_node_type::property;
      property_node.editor_position = node.editor_position - math::vector2{220.0f, -60.0f};
      property_node.property_id = property.id;
      new_nodes.push_back(property_node);

      description.edges.push_back(shader_graph_edge{property_node.id, 0u, node.id, 1u});
      continue;
    }

    if (!node.exposed) {
      continue;
    }

    const auto type = [&node]() -> std::optional<shader_graph_parameter_type> {
      switch (node.type) {
        case shader_node_type::constant_float: return shader_graph_parameter_type::float_value;
        case shader_node_type::constant_vector2: return shader_graph_parameter_type::vector2_value;
        case shader_node_type::constant_vector3: return shader_graph_parameter_type::vector3_value;
        case shader_node_type::constant_vector4: return shader_graph_parameter_type::vector4_value;
        case shader_node_type::constant_color: return shader_graph_parameter_type::color_value;
        default: return std::nullopt;
      }
    }();

    if (!type) {
      continue;
    }

    // One float4 per old parameter, so materials saved with the old layout read the same values.
    auto& property = add_property(node.name, "Property", *type, 4u * float_slot++);

    std::visit([&property]<typename T>(const T& value) {
      if constexpr (!std::is_same_v<T, std::string>) {
        property.value = value;
      }
    }, node.value);

    description.properties.push_back(shader_graph_property_description{property, std::string{}});

    node.type = shader_node_type::property;
    node.property_id = property.id;
    node.value = std::monostate{};
  }

  std::ranges::move(new_nodes, std::back_inserter(description.nodes));
}

static auto parse_property_value(const YAML::Node& node, shader_graph_parameter_type type) -> shader_graph_node_value {
  if (!node) {
    return type == shader_graph_parameter_type::color_value ? shader_graph_node_value{math::color{1.0f, 1.0f, 1.0f, 1.0f}} : shader_graph_node_value{};
  }

  switch (type) {
    case shader_graph_parameter_type::float_value: return node.as<std::float_t>();
    case shader_graph_parameter_type::vector2_value: return node.as<math::vector2>();
    case shader_graph_parameter_type::vector3_value: return node.as<math::vector3>();
    case shader_graph_parameter_type::vector4_value: return node.as<math::vector4>();
    case shader_graph_parameter_type::color_value: return node.as<math::color>();
    case shader_graph_parameter_type::texture_value: return std::monostate{};
  }

  return std::monostate{};
}

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
      if (node_yaml["preview"]) node.preview = node_yaml["preview"].as<bool>();
      if (node_yaml["property"]) node.property_id = node_yaml["property"].as<std::uint32_t>();

      switch (node.type) {
        case shader_node_type::constant_float:
          node.value = node_yaml["value"] ? node_yaml["value"].as<std::float_t>() : 0.0f;
          break;
        case shader_node_type::constant_vector2:
          node.value = node_yaml["value"] ? node_yaml["value"].as<math::vector2>() : math::vector2{};
          break;
        case shader_node_type::constant_vector3:
          node.value = node_yaml["value"] ? node_yaml["value"].as<math::vector3>() : math::vector3{};
          break;
        case shader_node_type::constant_vector4:
          node.value = node_yaml["value"] ? node_yaml["value"].as<math::vector4>() : math::vector4{};
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
        case shader_node_type::scene_depth:
          node.value = node_yaml["mode"] ? node_yaml["mode"].as<std::string>() : std::string{"linear01"};
          break;
        case shader_node_type::screen_position:
          node.value = node_yaml["mode"] ? node_yaml["mode"].as<std::string>() : std::string{"default"};
          break;
        default:
          break; // monostate nodes carry no payload
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

  if (const auto properties = root["properties"]) {
    description.properties.reserve(properties.size());

    for (const auto property_yaml : properties) {
      auto entry = shader_graph_property_description{};
      auto& property = entry.property;

      if (property_yaml["id"]) property.id = property_yaml["id"].as<std::uint32_t>();
      if (property_yaml["name"]) property.name = property_yaml["name"].as<std::string>();
      if (property_yaml["reference"]) property.reference = property_yaml["reference"].as<std::string>();
      if (property_yaml["type"]) property.type = shader_graph_parameter_type_from_string(property_yaml["type"].as<std::string>());
      if (property_yaml["exposed"]) property.exposed = property_yaml["exposed"].as<bool>();
      if (property_yaml["slot"]) property.slot = property_yaml["slot"].as<std::uint32_t>();

      // Value properties are saved as a packed float offset; files from before packing have a float4 index under "slot" instead.
      if (property.type != shader_graph_parameter_type::texture_value) {
        property.slot = property_yaml["offset"] ? property_yaml["offset"].as<std::uint32_t>() : property.slot * 4u;
      }
      if (property_yaml["slider"]) property.float_mode = property_yaml["slider"].as<bool>() ? shader_graph_float_mode::slider : shader_graph_float_mode::plain;
      if (property_yaml["range"]) property.slider_range = property_yaml["range"].as<math::vector2>();
      if (property_yaml["hdr"]) property.is_hdr = property_yaml["hdr"].as<bool>();
      if (property_yaml["linear"]) property.is_linear = property_yaml["linear"].as<bool>();
      if (property_yaml["texture"]) entry.texture_path = property_yaml["texture"].as<std::string>();

      property.value = parse_property_value(property_yaml["value"], property.type);

      description.properties.push_back(std::move(entry));
    }
  } else {
    migrate_to_properties(description);
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

  // Inside the engine's shaders tree so its #includes resolve. Named after graph_name, since a numeric file stem may not be a valid Slang module name.
  const auto directory = filesystem::engine_data_directory() / "shaders" / "generated";
  const auto path = directory / fmt::format("{}.slang", graph_name);

  auto error = std::error_code{};
  std::filesystem::create_directories(directory, error);

  // Older generations are unreachable once this one is written, so delete their files.
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
