// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/shader_graph_codegen.hpp>

using namespace sbx::assets;

// Pre-property format: exposed constants are parameters (float slots in node id order), every Sample Texture is a texture parameter.
static constexpr auto legacy_graph = R"(
name: legacy
nodes:
  - {id: 0, type: constant_color, name: Tint, exposed: true, value: {r: 1, g: 0, b: 0, a: 1}}
  - {id: 1, type: constant_float, name: Hidden, exposed: false, value: 2}
  - {id: 2, type: constant_float, name: Strength, exposed: true, value: 0.5}
  - {id: 3, type: texture_sample, name: Albedo, texture: ""}
  - {id: 4, type: output_fragment_unlit}
edges:
  - {from_node: 3, from_pin: 0, to_node: 4, to_pin: 0}
)";

TEST(shader_graph_properties_test, legacy_graph_migrates_to_properties_in_its_old_slots) {
  const auto path = std::filesystem::temp_directory_path() / "sbx_legacy.shadergraph";

  {
    auto out = std::ofstream{path};
    out << legacy_graph;
  }

  const auto description = asset_cooker::parse_shader_graph_file(path);
  std::filesystem::remove(path);

  ASSERT_TRUE(description.has_value());
  ASSERT_EQ(description->properties.size(), 3u);

  const auto& tint = description->properties[0].property;
  const auto& strength = description->properties[1].property;
  const auto& albedo = description->properties[2].property;

  EXPECT_EQ(tint.name, "Tint");
  EXPECT_EQ(tint.reference, "_Tint");
  EXPECT_EQ(tint.type, shader_property_type::color_value);
  // Old parameters keep one float4 each, so existing materials read the same values.
  EXPECT_EQ(tint.slot, 0u);
  EXPECT_EQ(strength.slot, 4u);
  EXPECT_EQ(albedo.type, shader_property_type::texture_value);
  EXPECT_EQ(albedo.slot, 0u);

  const auto node = [&](std::uint32_t id) { return *std::ranges::find(description->nodes, id, &shader_graph_node_description::id); };

  EXPECT_EQ(node(0u).type, shader_node_type::property);
  EXPECT_EQ(node(0u).property_id, tint.id);
  EXPECT_EQ(node(1u).type, shader_node_type::constant_float);

  // The Sample Texture got a property node wired into its Texture pin.
  const auto texture_edge = std::ranges::find_if(description->edges, [](const auto& edge) { return edge.to_node == 3u && edge.to_pin == 1u; });
  ASSERT_NE(texture_edge, description->edges.end());
  EXPECT_EQ(node(texture_edge->from_node).property_id, albedo.id);
}

TEST(shader_graph_properties_test, slots_stay_stable_and_references_unique) {
  auto properties = std::vector<shader_property>{};

  const auto add = [&](const std::string& name, shader_property_type type) {
    auto property = shader_property{};
    property.id = shader_graph_next_property_id(properties);
    property.name = name;
    property.reference = shader_graph_make_reference(name, properties);
    property.type = type;
    property.slot = shader_graph_next_property_slot(properties, type);
    properties.push_back(property);
  };

  add("Base Color", shader_property_type::color_value);
  add("Base Color", shader_property_type::float_value);
  add("2nd map", shader_property_type::texture_value);

  EXPECT_EQ(properties[0].reference, "_BaseColor");
  EXPECT_EQ(properties[1].reference, "_BaseColor_1");
  EXPECT_EQ(properties[2].reference, "_P2ndMap");

  EXPECT_EQ(properties[0].slot, 0u);
  EXPECT_EQ(properties[1].slot, 4u); // the color took floats 0-3
  EXPECT_EQ(properties[2].slot, 0u); // textures count separately

  // Deleting and reordering never moves a slot; the freed one is reused.
  properties.erase(properties.begin());
  std::ranges::reverse(properties);
  EXPECT_EQ(std::ranges::find(properties, std::string{"_BaseColor_1"}, &shader_property::reference)->slot, 4u);

  add("Roughness", shader_property_type::float_value);
  EXPECT_EQ(properties.back().slot, 0u);
}

TEST(shader_graph_properties_test, texture_property_feeds_sample_texture) {
  auto graph = shader_graph::create_info{};

  auto property = shader_property{};
  property.id = 1u;
  property.name = "Albedo";
  property.reference = "_Albedo";
  property.type = shader_property_type::texture_value;
  property.slot = 3u;
  graph.properties.push_back(property);

  graph.nodes = {
    shader_graph_node{.id = 0u, .type = shader_node_type::property, .property_id = 1u},
    shader_graph_node{.id = 1u, .type = shader_node_type::texture_sample},
    shader_graph_node{.id = 2u, .type = shader_node_type::texture_sample},
    shader_graph_node{.id = 3u, .type = shader_node_type::output_fragment_unlit}
  };

  graph.edges = {
    shader_graph_edge{0u, 0u, 1u, 1u},
    shader_graph_edge{1u, 0u, 3u, 0u}
  };

  // Node 2 is inline: the lowest texture slot the property doesn't use.
  const auto parameters = compute_shader_graph_parameters(graph.properties, graph.nodes, graph.edges);
  ASSERT_EQ(parameters.size(), 2u);
  EXPECT_EQ(parameters[1].node_id, 2u);
  EXPECT_EQ(parameters[1].slot, 0u);
  EXPECT_FALSE(parameters[1].exposed);

  const auto source = generate_shader_graph_source("test", graph);
  ASSERT_TRUE(source.has_value()) << source.error();
  EXPECT_NE(source->find("uint property_0 = material.generic_textures[3];"), std::string::npos);
  EXPECT_NE(source->find("textures[property_0].Sample("), std::string::npos);

  // A texture can't feed a math node.
  graph.edges.push_back(shader_graph_edge{0u, 0u, 3u, 1u});
  EXPECT_FALSE(generate_shader_graph_source("test", graph).has_value());
}

TEST(shader_graph_properties_test, values_pack_by_component_without_straddling_float4s) {
  auto properties = std::vector<shader_property>{};

  const auto add = [&](shader_property_type type) {
    auto property = shader_property{};
    property.id = shader_graph_next_property_id(properties);
    property.type = type;
    property.slot = shader_graph_next_property_slot(properties, type);
    properties.push_back(property);
    return property.slot;
  };

  EXPECT_EQ(add(shader_property_type::float_value), 0u);
  EXPECT_EQ(add(shader_property_type::vector2_value), 2u);
  EXPECT_EQ(add(shader_property_type::float_value), 1u);
  EXPECT_EQ(add(shader_property_type::vector3_value), 4u);
  EXPECT_EQ(add(shader_property_type::float_value), 7u);
  EXPECT_EQ(add(shader_property_type::color_value), 8u);

  EXPECT_EQ(shader_graph_parameter_expression(shader_property_type::vector2_value, 2u), "material.generic_params[0].zw");
  EXPECT_EQ(shader_graph_parameter_expression(shader_property_type::float_value, 7u), "material.generic_params[1].w");
  EXPECT_EQ(shader_graph_parameter_expression(shader_property_type::color_value, 8u), "material.generic_params[2]");

  // Writing one packed value leaves its neighbours alone.
  auto params = std::array<sbx::math::vector4, material_max_params>{};
  params[0] = sbx::math::vector4{1.0f, 2.0f, 3.0f, 4.0f};

  shader_property_write(params, shader_property_type::vector2_value, 2u, sbx::math::vector4{8.0f, 9.0f, 0.0f, 0.0f});
  EXPECT_EQ(params[0], (sbx::math::vector4{1.0f, 2.0f, 8.0f, 9.0f}));
  EXPECT_EQ(shader_property_read(params, shader_property_type::float_value, 1u).x(), 2.0f);

  // 32 Floats fill the material exactly.
  auto floats = std::vector<shader_property>{};

  for (auto index = 0u; index < material_max_param_components; ++index) {
    ASSERT_TRUE(shader_graph_has_room_for(floats, shader_property_type::float_value));
    floats.push_back(shader_property{.id = index + 1u, .type = shader_property_type::float_value, .slot = shader_graph_next_property_slot(floats, shader_property_type::float_value)});
  }

  EXPECT_FALSE(shader_graph_has_room_for(floats, shader_property_type::float_value));
}
