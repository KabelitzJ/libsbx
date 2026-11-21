// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

// No mesh or material, so the scenes load without an engine.

namespace scenes = sbx::scenes;
namespace math = sbx::math;

auto make_instances(std::size_t count) -> std::vector<scenes::instance_data> {
  auto instances = std::vector<scenes::instance_data>(count);

  for (auto index = std::size_t{0u}; index < count; ++index) {
    const auto offset = static_cast<std::float_t>(index);

    instances[index].rows[0] = math::vector4{1.0f, 0.0f, 0.0f, offset * 1.5f};
    instances[index].rows[1] = math::vector4{0.0f, 1.0f, 0.0f, 0.25f};
    instances[index].rows[2] = math::vector4{0.0f, 0.0f, 1.0f, -offset};
    instances[index].color = math::color{0.5f, 0.75f, 1.0f, 1.0f};
    instances[index].custom_data = math::vector4{offset, 2.0f, 3.0f, 4.0f};
  }

  return instances;
}

TEST(instanced_mesh_renderer, instances_survive_a_save_and_load_exactly) {
  auto source = scenes::scene{"Source"};
  auto field = source.create_node("Field");

  auto& renderer = field.add_component<scenes::instanced_mesh_renderer>();
  renderer.lod_bias = 2.0f;
  renderer.data = std::make_shared<const std::vector<scenes::instance_data>>(make_instances(257u));

  const auto text = scenes::scene_serializer::serialize(source);

  auto loaded = scenes::scene{"Loaded"};
  scenes::scene_serializer::load(loaded, YAML::Load(text));

  const auto loaded_field = loaded.find(field.id());
  ASSERT_TRUE(loaded_field.is_valid());

  const auto& loaded_renderer = loaded_field.get_component<scenes::instanced_mesh_renderer>();
  ASSERT_NE(loaded_renderer.data, nullptr);
  ASSERT_EQ(loaded_renderer.data->size(), renderer.data->size());
  EXPECT_EQ(std::memcmp(loaded_renderer.data->data(), renderer.data->data(), renderer.data->size() * sizeof(scenes::instance_data)), 0);
  EXPECT_EQ(loaded_renderer.lod_bias, 2.0f);

  // Same instances, same text: the editor's unsaved-changes check stays quiet.
  EXPECT_EQ(scenes::scene_serializer::serialize(loaded), text);
}

TEST(instanced_mesh_renderer, an_empty_renderer_writes_no_instances) {
  auto source = scenes::scene{"Source"};
  source.create_node("Empty").add_component<scenes::instanced_mesh_renderer>();

  const auto text = scenes::scene_serializer::serialize(source);

  EXPECT_NE(text.find("instanced_mesh_renderer"), std::string::npos);
  EXPECT_EQ(text.find("instances:"), std::string::npos);
}
