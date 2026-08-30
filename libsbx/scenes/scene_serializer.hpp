// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
#define LIBSBX_SCENES_SCENE_SERIALIZER_HPP_

#include <filesystem>
#include <string>

#include <libsbx/scenes/scene.hpp>

namespace YAML { class Node; } // avoids pulling yaml-cpp's full header into every scene_serializer.hpp include

namespace sbx::scenes {

class scene_serializer final {

public:

  scene_serializer() = delete;

  static auto save(scene& target, const std::filesystem::path& path) -> void;

  /** @brief Renders target to the same YAML save() would write, without touching disk. */
  [[nodiscard]] static auto serialize(scene& target) -> std::string;

  static auto load(scene& target, const std::filesystem::path& path) -> void;

  /** @brief Snapshots subtree_root and its whole descendant subtree — components, structure, ids — self-contained (own asset table), for undo/redo. */
  [[nodiscard]] static auto serialize_subtree(scene& target, node subtree_root) -> YAML::Node;

  /** @brief Recreates a serialize_subtree() snapshot under target._root; caller repositions it (see scene::insert_child). Returns the recreated root. */
  static auto deserialize_subtree(scene& target, const YAML::Node& snapshot) -> node;

private:

  [[nodiscard]] static auto _build(scene& target) -> YAML::Node;

}; // class scene_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
