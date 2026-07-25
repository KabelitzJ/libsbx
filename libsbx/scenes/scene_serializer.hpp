// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
#define LIBSBX_SCENES_SCENE_SERIALIZER_HPP_

#include <filesystem>

#include <libsbx/scenes/scene.hpp>

namespace sbx::scenes {

class scene_serializer final {

public:

  scene_serializer() = delete;

  static auto save(scene& target, const std::filesystem::path& path) -> void;

  static auto load(scene& target, const std::filesystem::path& path) -> void;

}; // class scene_serializer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_SERIALIZER_HPP_
