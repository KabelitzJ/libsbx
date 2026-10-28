// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/assets/shader_reflection.hpp>

using namespace sbx::assets;

using module_reflection = sbx::graphics::shader_compiler::module_reflection;
using reflected_field = sbx::graphics::shader_compiler::reflected_field;
using reflected_attribute = sbx::graphics::shader_compiler::reflected_attribute;

static auto float_field(std::string name, std::uint32_t component_count, std::vector<reflected_attribute> attributes = {}) -> reflected_field {
  return reflected_field{.name = std::move(name), .type = reflected_field::kind::float_vector, .component_count = component_count, .type_name = "float", .attributes = std::move(attributes)};
}

static auto texture_field(std::string name, std::vector<reflected_attribute> attributes = {}, reflected_field::kind kind = reflected_field::kind::texture2d) -> reflected_field {
  return reflected_field{.name = std::move(name), .type = kind, .type_name = "Texture2D", .attributes = std::move(attributes)};
}

TEST(shader_property_test, one_struct_mixes_values_and_textures_and_values_may_straddle) {
  auto reflection = module_reflection{};
  reflection.structs["properties"] = {
    float_field("strength", 1u, {{"Property", {std::string{"_Strength"}, std::string{"Strength"}}}, {"Range", {0.0f, 2.0f}}, {"Default", {0.5f, 0.0f, 0.0f, 0.0f}}}),
    texture_field("albedo", {{"Property", {std::string{"_Albedo"}, std::string{"Albedo"}}}}),
    float_field("tint", 4u, {{"Color", {}}, {"Hdr", {}}, {"Default", {1.0f, 0.5f, 0.25f, 1.0f}}}),
    float_field("scroll", 2u),
    texture_field("mask", {{"Linear", {}}}),
    float_field("offset", 3u),
    texture_field("layers", {}, reflected_field::kind::texture2d_array)
  };

  const auto result = shader_properties_from_reflection(reflection);

  ASSERT_TRUE(result.has_value()) << result.error();

  const auto& properties = result->properties;
  ASSERT_EQ(properties.size(), 7u);

  const auto& strength = properties[0];
  EXPECT_EQ(strength.reference, "_Strength");
  EXPECT_EQ(strength.name, "Strength");
  EXPECT_EQ(strength.type, shader_property_type::float_value);
  EXPECT_EQ(strength.slot, 0u);
  EXPECT_EQ(strength.float_mode, shader_property_float_mode::slider);
  EXPECT_FLOAT_EQ(strength.slider_range.y(), 2.0f);
  EXPECT_FLOAT_EQ(std::get<std::float_t>(strength.value), 0.5f);

  EXPECT_EQ(properties[1].reference, "_Albedo");
  EXPECT_EQ(properties[1].type, shader_property_type::texture_value);
  EXPECT_EQ(properties[1].slot, 0u);

  const auto& tint = properties[2];
  EXPECT_EQ(tint.reference, "_tint");
  EXPECT_EQ(tint.type, shader_property_type::color_value);
  EXPECT_EQ(tint.slot, 1u); // textures don't advance the float count; straddles generic_params[0] and [1]
  EXPECT_TRUE(tint.is_hdr);

  EXPECT_EQ(properties[3].type, shader_property_type::vector2_value);
  EXPECT_EQ(properties[3].slot, 5u);
  EXPECT_EQ(properties[4].slot, 1u);
  EXPECT_TRUE(properties[4].is_linear);
  EXPECT_EQ(properties[5].type, shader_property_type::vector3_value);
  EXPECT_EQ(properties[5].slot, 7u);
  EXPECT_EQ(properties[6].slot, 2u);
  EXPECT_TRUE(properties[6].is_texture_array);

  // The loader reads each property's own floats and texture slot.
  const auto& loader = result->material_loader;
  EXPECT_NE(loader.find("properties load_properties(material_data material) {"), std::string::npos);
  EXPECT_NE(loader.find("result.strength = material.generic_params[0].x;"), std::string::npos);
  EXPECT_NE(loader.find("result.tint = float4(material.generic_params[0].y, material.generic_params[0].z, material.generic_params[0].w, material.generic_params[1].x);"), std::string::npos);
  EXPECT_NE(loader.find("result.offset = float3(material.generic_params[1].w, material.generic_params[2].x, material.generic_params[2].y);"), std::string::npos);
  EXPECT_NE(loader.find("result.albedo = textures[material.generic_textures[0]];"), std::string::npos);
  EXPECT_NE(loader.find("result.layers = texture_arrays[material.generic_textures[2]];"), std::string::npos);

  // The straddling color writes and reads back through the flat float view.
  auto params = shader_property_default_params(properties);
  EXPECT_FLOAT_EQ(params[0].x(), 0.5f);
  EXPECT_FLOAT_EQ(params[0].y(), 1.0f);
  EXPECT_FLOAT_EQ(params[1].x(), 1.0f); // tint.a at float 4

  const auto read = shader_property_read(params, tint.type, tint.slot);
  EXPECT_FLOAT_EQ(read.y(), 0.5f);
  EXPECT_FLOAT_EQ(read.z(), 0.25f);
}

TEST(shader_property_test, unusable_structs_are_rejected) {
  auto over_budget = module_reflection{};

  for (auto i = 0u; i < 9u; ++i) {
    over_budget.structs["properties"].push_back(float_field("v" + std::to_string(i), 4u));
  }

  EXPECT_FALSE(shader_properties_from_reflection(over_budget).has_value());

  auto too_many_textures = module_reflection{};

  for (auto i = 0u; i < 17u; ++i) {
    too_many_textures.structs["properties"].push_back(texture_field("t" + std::to_string(i)));
  }

  EXPECT_FALSE(shader_properties_from_reflection(too_many_textures).has_value());

  auto unsupported = module_reflection{};
  unsupported.structs["properties"] = {reflected_field{.name = "nested", .type = reflected_field::kind::structure, .type_name = "inner"}};
  EXPECT_FALSE(shader_properties_from_reflection(unsupported).has_value());

  auto duplicate = module_reflection{};
  duplicate.structs["properties"] = {float_field("a", 1u, {{"Property", {std::string{"_X"}, std::string{"A"}}}}), float_field("b", 1u, {{"Property", {std::string{"_X"}, std::string{"B"}}}})};
  EXPECT_FALSE(shader_properties_from_reflection(duplicate).has_value());

  const auto none = shader_properties_from_reflection(module_reflection{});
  ASSERT_TRUE(none.has_value());
  EXPECT_TRUE(none->properties.empty());
  EXPECT_TRUE(none->material_loader.empty());
}

// Self-contained (no engine includes), so only the engine data directory has to exist for the session's search paths.
static constexpr auto reflected_source = std::string_view{R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct PropertyAttribute {
  string reference;
  string name;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct ColorAttribute {
};

[__AttributeUsage(_AttributeTargets.Var)]
struct RangeAttribute {
  float min;
  float max;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct DefaultAttribute {
  float x;
  float y;
  float z;
  float w;
};

struct properties {
  [Property("_Strength", "Strength"), Range(0, 1), Default(0.5, 0, 0, 0)] float strength;
  Texture2D albedo;
  [Property("_Tint", "Tint"), Color, Default(1, 1, 1, 1)] float4 tint;
  Texture2DArray layers;
};

struct push_data {
  float4* output;
};

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  push.output[0] = float4(1.0);
}
)"};

TEST(shader_property_test, slang_reflection_reads_struct_fields_and_attributes) {
  try {
    static_cast<void>(sbx::filesystem::engine_data_directory());
  } catch (const std::exception&) {
    GTEST_SKIP() << "engine data directory not found";
  }

  const auto path = std::filesystem::temp_directory_path() / "sbx_shader_property_test.slang";

  {
    auto out = std::ofstream{path};
    out << reflected_source;
  }

  auto compiler = sbx::graphics::shader_compiler{};

  static constexpr auto entry_points = std::array<std::string_view, 2u>{"vertex_main", "compute_main"};
  static constexpr auto structs = std::array<std::string_view, 1u>{"properties"};

  const auto reflection = compiler.reflect_module(path, entry_points, structs);
  std::filesystem::remove(path);

  ASSERT_EQ(reflection.entry_points, std::vector<std::string>{"compute_main"});

  const auto result = shader_properties_from_reflection(reflection);

  ASSERT_TRUE(result.has_value()) << result.error();

  const auto& properties = result->properties;
  ASSERT_EQ(properties.size(), 4u);

  EXPECT_EQ(properties[0].reference, "_Strength");
  EXPECT_EQ(properties[0].float_mode, shader_property_float_mode::slider);
  EXPECT_FLOAT_EQ(properties[0].slider_range.y(), 1.0f);
  EXPECT_FLOAT_EQ(std::get<std::float_t>(properties[0].value), 0.5f);

  EXPECT_EQ(properties[1].reference, "_albedo");
  EXPECT_EQ(properties[1].type, shader_property_type::texture_value);
  EXPECT_FALSE(properties[1].is_texture_array);

  EXPECT_EQ(properties[2].reference, "_Tint");
  EXPECT_EQ(properties[2].type, shader_property_type::color_value);
  EXPECT_EQ(properties[2].slot, 1u);

  EXPECT_TRUE(properties[3].is_texture_array);
  EXPECT_EQ(properties[3].slot, 1u);
}
