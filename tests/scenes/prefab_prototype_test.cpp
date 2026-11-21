// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include <gtest/gtest.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/prefab.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/prefab_prototype.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

// Asset-free components only, so no running engine is needed.

namespace scenes = sbx::scenes;
namespace assets = sbx::assets;
namespace math = sbx::math;

// A three-node prefab (Unit > Body > Head) built from a scene, the way the editor's Create Prefab does.
auto make_unit_prefab() -> assets::prefab_handle {
  auto source = scenes::scene{"Source"};

  auto unit = source.create_node("Unit");
  auto body = source.create_node("Body");
  auto head = source.create_node("Head");

  body.set_parent(unit);
  head.set_parent(body);
  head.add_component<scenes::camera>().fov_degrees = 70.0f;

  return assets::prefab_handle{std::make_shared<assets::prefab>(scenes::scene_serializer::serialize_subtree(source, unit), math::uuid::create(), "Unit")};
}

TEST(prefab_prototype, copies_values_and_presence) {
  auto scene = scenes::scene{"Test"};

  auto prototype = scene.create_node("Prototype", scenes::local_transform{math::vector3{1.0f, 2.0f, 3.0f}});
  prototype.add_component<scenes::camera>().fov_degrees = 42.0f;
  prototype.add_component<scenes::point_light>().range = 7.0f;
  prototype.get_component<scenes::layer>().index = 3u;
  prototype.set_active(false);

  auto copy = scene.create_node("Copy");
  scenes::copy_prefab_components(prototype, copy);

  EXPECT_EQ(copy.get_component<scenes::local_transform>().position, (math::vector3{1.0f, 2.0f, 3.0f}));
  EXPECT_EQ(copy.get_component<scenes::camera>().fov_degrees, 42.0f);
  EXPECT_EQ(copy.get_component<scenes::point_light>().range, 7.0f);
  EXPECT_EQ(copy.get_component<scenes::layer>().index, 3u);
  EXPECT_FALSE(copy.is_active());
  EXPECT_FALSE(copy.has_component<scenes::spot_light>());

  // Identity stays the copy's own.
  EXPECT_NE(copy.id(), prototype.id());
  EXPECT_EQ(copy.name().str(), "Copy");
}

TEST(prefab_prototype, remaps_references_inside_the_prefab_only) {
  auto scene = scenes::scene{"Test"};

  auto node = scene.create_node("Node");

  const auto inside = math::uuid::create();
  const auto outside = math::uuid::create();
  const auto inside_copy = math::uuid::create();

  auto& toggle = node.add_component<sbx::canvas::ui_toggle>();
  toggle.group = inside;

  auto& scripts = node.add_component<scenes::script_component>();
  auto& entry = scripts.scripts.emplace_back();
  entry.class_name = "Squad";
  entry.field_overrides.push_back(scenes::script_field_override{.name = "leader", .type = scenes::script_field_type::node, .node_value = inside});
  entry.field_overrides.push_back(scenes::script_field_override{.name = "target", .type = scenes::script_field_type::node, .node_value = outside});

  scenes::remap_prefab_node_references(node, std::unordered_map<math::uuid, math::uuid>{{inside, inside_copy}});

  EXPECT_EQ(node.get_component<sbx::canvas::ui_toggle>().group, inside_copy);

  const auto& fields = node.get_component<scenes::script_component>().scripts.front().field_overrides;
  EXPECT_EQ(fields[0].node_value, inside_copy);
  EXPECT_EQ(fields[1].node_value, outside);
}

TEST(prefab_prototype, reports_uncopied_components) {
  const auto node_yaml = YAML::Load(R"(
    id: 1
    tag: Unit
    components:
      - type: transform
      - type: camera
      - type: teleporter
  )");

  const auto uncopied = scenes::uncopied_prefab_component_keys(node_yaml);

  ASSERT_EQ(uncopied.size(), 1u);
  EXPECT_EQ(uncopied.front(), "teleporter");
}

TEST(prefab_prototype_cache, builds_once_per_version) {
  auto cache = scenes::prefab_prototype_cache{};
  const auto prefab = make_unit_prefab();

  const auto& first = cache.prototype_of(prefab);

  ASSERT_NE(first.nodes_scene, nullptr);
  EXPECT_EQ(first.node_ids.size(), 3u);

  // Same version: the same prototype, not a rebuild.
  const auto& second = cache.prototype_of(prefab);
  EXPECT_EQ(&first, &second);
  EXPECT_EQ(first.nodes_scene.get(), second.nodes_scene.get());
  EXPECT_EQ(cache.size(), 1u);
}

TEST(prefab_prototype_cache, orders_parents_first_and_keeps_prefab_content) {
  auto cache = scenes::prefab_prototype_cache{};
  const auto prefab = make_unit_prefab();

  const auto& prototype = cache.prototype_of(prefab);
  auto& scene = *prototype.nodes_scene;

  ASSERT_EQ(prototype.node_ids.front(), prototype.root_id);

  for (auto index = std::size_t{1u}; index < prototype.node_ids.size(); ++index) {
    auto current = scene.find(prototype.node_ids[index]);
    const auto parent = scene.node_of(current.get_component<scenes::relationship>().parent);

    // Every parent appears before its child.
    const auto parent_position = std::ranges::find(prototype.node_ids, static_cast<math::uuid>(parent.id())) - prototype.node_ids.begin();
    EXPECT_LT(static_cast<std::size_t>(parent_position), index);

    EXPECT_TRUE(current.has_component<scenes::prefab_member>());
  }

  auto head = scene.find(prototype.node_ids.back());
  EXPECT_EQ(head.name().str(), "Head");
  EXPECT_EQ(head.get_component<scenes::camera>().fov_degrees, 70.0f);

  // The root doesn't reference its own prefab, or the prototype would keep it alive.
  EXPECT_FALSE(scene.find(prototype.root_id).has_component<scenes::prefab_instance>());
}

TEST(prefab_prototype_cache, prunes_prototypes_of_released_prefabs) {
  auto cache = scenes::prefab_prototype_cache{};

  // Created first and alive throughout, so the released one below can't reuse its address.
  const auto kept = make_unit_prefab();
  static_cast<void>(cache.prototype_of(kept));

  {
    const auto released = make_unit_prefab();
    static_cast<void>(cache.prototype_of(released));
  }

  ASSERT_EQ(cache.size(), 2u);

  cache.prune();

  EXPECT_EQ(cache.size(), 1u);
}

// The node under parent with the given name, or an invalid node.
auto child_named(scenes::scene& scene, scenes::node parent, const std::string& name) -> scenes::node {
  for (const auto child : parent.get_component<scenes::relationship>().children) {
    if (auto node = scene.node_of(child); node.name().str() == name) {
      return node;
    }
  }

  return scenes::node{};
}

TEST(instantiate_prefab_prototype, builds_the_same_tree_with_fresh_ids) {
  auto cache = scenes::prefab_prototype_cache{};
  auto target = scenes::scene{"Target"};
  const auto prefab = make_unit_prefab();

  auto first = scenes::instantiate_prefab_prototype(target, prefab, cache);
  auto second = scenes::instantiate_prefab_prototype(target, prefab, cache);

  ASSERT_TRUE(first.is_valid());
  ASSERT_TRUE(second.is_valid());
  EXPECT_NE(first.id(), second.id());

  for (auto root : {first, second}) {
    EXPECT_EQ(root.name().str(), "Unit");
    ASSERT_TRUE(root.has_component<scenes::prefab_instance>());
    EXPECT_EQ(root.get_component<scenes::prefab_instance>().source.get(), prefab.get());

    auto body = child_named(target, root, "Body");
    ASSERT_TRUE(body.is_valid());

    auto head = child_named(target, body, "Head");
    ASSERT_TRUE(head.is_valid());
    EXPECT_EQ(head.get_component<scenes::camera>().fov_degrees, 70.0f);
  }

  // Both instances map their nodes to the same prefab members.
  EXPECT_EQ(child_named(target, first, "Body").get_component<scenes::prefab_member>().member_id, child_named(target, second, "Body").get_component<scenes::prefab_member>().member_id);
}

TEST(instantiate_prefab_prototype, keeps_a_given_root_id) {
  auto cache = scenes::prefab_prototype_cache{};
  auto target = scenes::scene{"Target"};
  const auto root_id = math::uuid::create();

  auto root = scenes::instantiate_prefab_prototype(target, make_unit_prefab(), cache, root_id);

  ASSERT_TRUE(root.is_valid());
  EXPECT_EQ(static_cast<math::uuid>(root.id()), root_id);
}

TEST(instantiate_prefab_prototype, points_references_inside_the_prefab_at_the_instance) {
  auto source = scenes::scene{"Source"};

  auto unit = source.create_node("Unit");
  auto toggle_node = source.create_node("Toggle");
  auto group_node = source.create_node("Group");

  toggle_node.set_parent(unit);
  group_node.set_parent(unit);
  toggle_node.add_component<sbx::canvas::ui_toggle>().group = group_node.id();

  const auto prefab = assets::prefab_handle{std::make_shared<assets::prefab>(scenes::scene_serializer::serialize_subtree(source, unit), math::uuid::create(), "Toggles")};

  auto cache = scenes::prefab_prototype_cache{};
  auto target = scenes::scene{"Target"};

  auto root = scenes::instantiate_prefab_prototype(target, prefab, cache);
  ASSERT_TRUE(root.is_valid());

  auto toggle = child_named(target, root, "Toggle");
  auto group = child_named(target, root, "Group");
  ASSERT_TRUE(toggle.is_valid());
  ASSERT_TRUE(group.is_valid());

  EXPECT_EQ(toggle.get_component<sbx::canvas::ui_toggle>().group, static_cast<math::uuid>(group.id()));
}
