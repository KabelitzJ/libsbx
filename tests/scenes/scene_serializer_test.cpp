// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/prefab.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

// Only asset-free components are used here: scene_serializer fetches assets_module solely where
// an asset is resolved, so these scenes (and in-memory prefabs) need no running engine.

namespace scenes = sbx::scenes;
namespace assets = sbx::assets;
namespace math = sbx::math;

using serializer = scenes::scene_serializer;

auto load_from_string(scenes::scene& target, const std::string& text) -> void {
  serializer::load(target, YAML::Load(text));
}

auto child_tags(scenes::scene& scene, scenes::node parent) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (const auto child : parent.get_component<scenes::relationship>().children) {
    result.push_back(scene.node_of(child).name().str());
  }

  return result;
}

auto make_prefab(scenes::scene& source, scenes::node root) -> assets::prefab_handle {
  return assets::prefab_handle{std::make_shared<assets::prefab>(serializer::serialize_subtree(source, root), math::uuid::create(), "Test Prefab")};
}

// Swaps in an edited prefab and marks the instance stale, standing in for assets_module::update_prefab
// (which bumps the prefab's generation) -- sync_prefab_instances only resyncs when they differ.
auto replace_prefab(scenes::node instance_root, assets::prefab_handle edited) -> void {
  auto& instance = instance_root.get_component<scenes::prefab_instance>();
  instance.source = std::move(edited);
  instance.applied_generation = instance.source.generation() + 1u;
}

auto member_with_tag(scenes::scene& scene, scenes::node parent, const std::string& tag) -> scenes::node {
  for (const auto child : parent.get_component<scenes::relationship>().children) {
    if (auto node = scene.node_of(child); node.name().str() == tag) {
      return node;
    }
  }

  return scenes::node{};
}

TEST(scene_serializer_test, round_trip_preserves_hierarchy_ids_and_child_order) {
  auto source = scenes::scene{};

  auto a = source.create_node("A");
  auto b = source.create_node("B");

  for (const auto& name : {std::string{"A1"}, std::string{"A2"}, std::string{"A3"}}) {
    auto child = source.create_node(name);
    child.set_parent(a);
  }

  auto loaded = scenes::scene{};
  load_from_string(loaded, serializer::serialize(source));

  EXPECT_EQ(child_tags(loaded, loaded.root()), (std::vector<std::string>{"A", "B"}));

  auto loaded_a = loaded.find(a.id());
  ASSERT_TRUE(loaded_a.is_valid());
  EXPECT_EQ(child_tags(loaded, loaded_a), (std::vector<std::string>{"A1", "A2", "A3"}));

  EXPECT_TRUE(loaded.find(b.id()).is_valid());
}

TEST(scene_serializer_test, round_trip_preserves_transform_layer_and_active_state) {
  auto source = scenes::scene{};

  auto node = source.create_node("Moved");
  node.transform().position = math::vector3{1.5f, -2.0f, 0.25f};
  node.transform().scale = math::vector3{2.0f, 4.0f, 0.5f};
  node.layer().index = 3u;
  node.set_active(false);

  auto loaded = scenes::scene{};
  load_from_string(loaded, serializer::serialize(source));

  auto loaded_node = loaded.find(node.id());
  ASSERT_TRUE(loaded_node.is_valid());

  const auto& transform = loaded_node.transform();
  EXPECT_FLOAT_EQ(transform.position.x(), 1.5f);
  EXPECT_FLOAT_EQ(transform.position.y(), -2.0f);
  EXPECT_FLOAT_EQ(transform.position.z(), 0.25f);
  EXPECT_FLOAT_EQ(transform.scale.x(), 2.0f);
  EXPECT_FLOAT_EQ(transform.scale.y(), 4.0f);
  EXPECT_FLOAT_EQ(transform.scale.z(), 0.5f);

  EXPECT_EQ(loaded_node.layer().index, 3u);
  EXPECT_FALSE(loaded_node.is_active());
}

TEST(scene_serializer_test, round_trip_preserves_lights_and_scene_metadata) {
  auto source = scenes::scene{"Lit Scene"};

  auto lamp = source.create_node("Lamp");
  auto& light = lamp.add_component<scenes::point_light>();
  light.color = math::color{0.5f, 0.25f, 1.0f, 1.0f};
  light.intensity = 12.5f;
  light.range = 4.0f;

  source.set_primary_light(lamp);

  auto loaded = scenes::scene{};
  load_from_string(loaded, serializer::serialize(source));

  EXPECT_EQ(loaded.name(), "Lit Scene");

  auto loaded_lamp = loaded.find(lamp.id());
  ASSERT_TRUE(loaded_lamp.is_valid());
  ASSERT_TRUE(loaded_lamp.has_component<scenes::point_light>());

  const auto& loaded_light = loaded_lamp.get_component<scenes::point_light>();
  EXPECT_EQ(loaded_light.color, (math::color{0.5f, 0.25f, 1.0f, 1.0f}));
  EXPECT_FLOAT_EQ(loaded_light.intensity, 12.5f);
  EXPECT_FLOAT_EQ(loaded_light.range, 4.0f);

  ASSERT_TRUE(loaded.has_primary_light());
  EXPECT_EQ(loaded.primary_light().id(), lamp.id());
}

TEST(scene_serializer_test, serializing_a_loaded_scene_reproduces_the_same_text) {
  auto source = scenes::scene{"Stable"};

  auto parent = source.create_node("Parent");
  parent.transform().position = math::vector3{3.0f, 0.0f, -1.0f};

  auto child = source.create_node("Child");
  child.set_parent(parent);
  child.add_component<scenes::spot_light>().range = 7.0f;

  const auto first = serializer::serialize(source);

  auto loaded = scenes::scene{};
  load_from_string(loaded, first);

  EXPECT_EQ(serializer::serialize(loaded), first);
}

TEST(scene_serializer_test, instantiate_prefab_gives_fresh_ids_linked_to_their_members) {
  auto source = scenes::scene{};

  auto source_root = source.create_node("Lamp Post");
  auto source_bulb = source.create_node("Bulb");
  source_bulb.set_parent(source_root);
  source_bulb.add_component<scenes::point_light>().intensity = 10.0f;

  const auto prefab = make_prefab(source, source_root);

  auto target = scenes::scene{};
  auto instance = serializer::instantiate_prefab(target, prefab);

  ASSERT_TRUE(instance.is_valid());
  ASSERT_TRUE(instance.has_component<scenes::prefab_instance>());

  EXPECT_NE(instance.id(), source_root.id());
  EXPECT_EQ(instance.get_component<scenes::prefab_member>().member_id, source_root.id());

  auto bulb = member_with_tag(target, instance, "Bulb");
  ASSERT_TRUE(bulb.is_valid());

  EXPECT_NE(bulb.id(), source_bulb.id());
  EXPECT_EQ(bulb.get_component<scenes::prefab_member>().member_id, source_bulb.id());
  EXPECT_FLOAT_EQ(bulb.get_component<scenes::point_light>().intensity, 10.0f);
}

TEST(scene_serializer_test, sync_follows_prefab_edits_but_keeps_overridden_components) {
  auto source = scenes::scene{};

  auto source_root = source.create_node("Lights");
  auto source_lamp = source.create_node("Lamp");
  auto source_spot = source.create_node("Spot");
  source_lamp.set_parent(source_root);
  source_spot.set_parent(source_root);
  source_lamp.add_component<scenes::point_light>().intensity = 10.0f;
  source_spot.add_component<scenes::spot_light>().intensity = 10.0f;

  auto target = scenes::scene{};
  auto instance = serializer::instantiate_prefab(target, make_prefab(source, source_root));

  auto lamp = member_with_tag(target, instance, "Lamp");
  auto spot = member_with_tag(target, instance, "Spot");
  ASSERT_TRUE(lamp.is_valid());
  ASSERT_TRUE(spot.is_valid());

  lamp.get_component<scenes::point_light>().intensity = 99.0f;
  serializer::mark_prefab_override(target, lamp, "point_light", scenes::prefab_override_kind::component_value);

  source_lamp.get_component<scenes::point_light>().intensity = 50.0f;
  source_spot.get_component<scenes::spot_light>().intensity = 60.0f;
  replace_prefab(instance, make_prefab(source, source_root));

  serializer::sync_prefab_instances(target);

  EXPECT_FLOAT_EQ(lamp.get_component<scenes::point_light>().intensity, 99.0f);
  EXPECT_FLOAT_EQ(spot.get_component<scenes::spot_light>().intensity, 60.0f);
}

TEST(scene_serializer_test, sync_adds_nodes_the_prefab_gained) {
  auto source = scenes::scene{};

  auto source_root = source.create_node("Root");

  auto target = scenes::scene{};
  auto instance = serializer::instantiate_prefab(target, make_prefab(source, source_root));

  auto added = source.create_node("Added");
  added.set_parent(source_root);
  replace_prefab(instance, make_prefab(source, source_root));

  serializer::sync_prefab_instances(target);

  auto instance_added = member_with_tag(target, instance, "Added");
  ASSERT_TRUE(instance_added.is_valid());
  EXPECT_EQ(instance_added.get_component<scenes::prefab_member>().member_id, added.id());
}

TEST(scene_serializer_test, revert_restores_the_prefab_value_and_drops_the_override) {
  auto source = scenes::scene{};

  auto source_root = source.create_node("Root");
  auto source_lamp = source.create_node("Lamp");
  source_lamp.set_parent(source_root);
  source_lamp.add_component<scenes::point_light>().intensity = 10.0f;

  auto target = scenes::scene{};
  auto instance = serializer::instantiate_prefab(target, make_prefab(source, source_root));

  auto lamp = member_with_tag(target, instance, "Lamp");
  ASSERT_TRUE(lamp.is_valid());

  lamp.get_component<scenes::point_light>().intensity = 99.0f;
  serializer::mark_prefab_override(target, lamp, "point_light", scenes::prefab_override_kind::component_value);
  ASSERT_EQ(serializer::prefab_overrides_of(target, lamp).size(), 1u);

  serializer::revert_prefab_override(target, lamp, "point_light");

  EXPECT_FLOAT_EQ(lamp.get_component<scenes::point_light>().intensity, 10.0f);
  EXPECT_TRUE(serializer::prefab_overrides_of(target, lamp).empty());
}

TEST(scene_serializer_test, the_instance_roots_transform_is_never_recorded_as_an_override) {
  auto source = scenes::scene{};
  auto source_root = source.create_node("Root");

  auto target = scenes::scene{};
  auto instance = serializer::instantiate_prefab(target, make_prefab(source, source_root));

  serializer::mark_prefab_override(target, instance, "transform", scenes::prefab_override_kind::component_value);

  EXPECT_TRUE(serializer::prefab_overrides_of(target, instance).empty());
}
