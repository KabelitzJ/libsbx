// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <libsbx/core/project.hpp>

using namespace sbx::core;

TEST(project_scene_list_test, resolves_enabled_scenes_by_name_path_and_build_index) {
  auto target = project{"/tmp/unused", "Test"};

  target.set_scene_list({
    scene_list_entry{"menus/Main.scene", true},
    scene_list_entry{"levels/forest.scene", false},
    scene_list_entry{"levels/cave.scene", true}
  });

  EXPECT_EQ(target.scene_count_in_build(), 2u);
  EXPECT_EQ(target.startup_scene(), std::filesystem::path{"menus/Main.scene"});

  // Disabled entries have no build index, so cave is 1.
  EXPECT_EQ(target.scene_at(1u), std::filesystem::path{"levels/cave.scene"});
  EXPECT_FALSE(target.scene_at(2u).has_value());
  EXPECT_EQ(target.build_index_of("levels/cave.scene"), 1u);
  EXPECT_FALSE(target.build_index_of("levels/forest.scene").has_value());

  EXPECT_EQ(target.resolve_scene("main"), std::filesystem::path{"menus/Main.scene"}); // names are case-insensitive
  EXPECT_EQ(target.resolve_scene("levels/cave"), std::filesystem::path{"levels/cave.scene"}); // the extension may be omitted
  EXPECT_EQ(target.resolve_scene("levels/cave.scene"), std::filesystem::path{"levels/cave.scene"});
  EXPECT_FALSE(target.resolve_scene("forest").has_value()); // disabled
  EXPECT_FALSE(target.resolve_scene("missing").has_value());
}

TEST(project_scene_list_test, a_v4_startup_scene_becomes_the_first_list_entry_and_round_trips) {
  const auto directory = std::filesystem::temp_directory_path() / "libsbx_project_scene_list_test";
  std::filesystem::create_directories(directory);

  const auto file = directory / project::file_name;

  {
    auto out = std::ofstream{file};
    out << "project:\n  format_version: 4\n  name: Old\n  startup_scene: levels/start.scene\n";
  }

  const auto loaded = project::load(file);

  ASSERT_EQ(loaded.scene_list().size(), 1u);
  EXPECT_EQ(loaded.startup_scene(), std::filesystem::path{"levels/start.scene"});

  loaded.save(file);

  const auto reloaded = project::load(file);

  ASSERT_EQ(reloaded.scene_list().size(), 1u);
  EXPECT_EQ(reloaded.scene_list()[0].path, std::filesystem::path{"levels/start.scene"});
  EXPECT_TRUE(reloaded.scene_list()[0].enabled);

  std::filesystem::remove_all(directory);
}
