// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_REFLECTION_HPP_
#define LIBSBX_ASSETS_SHADER_REFLECTION_HPP_

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/assets/shader.hpp>
#include <libsbx/assets/shader_property.hpp>

namespace sbx::assets {

/** @brief A code shader's material inputs, from its `properties` struct. */
struct reflected_properties {
  std::vector<shader_property> properties;
  // The load_properties function compile() appends to the source; empty without a `properties` struct.
  std::string material_loader;
}; // struct reflected_properties

/**
 * @brief Builds a code shader's material properties and the Slang that loads them from its reflected `properties` struct (see shaders/material_properties.slang).
 *
 * Each value field's slot is the float count of the value fields before it, so a value may straddle two generic_params entries; each texture field's slot is the texture field count before it.
 *
 * @param reflection The shader's reflection; no `properties` struct means no properties and no loader.
 *
 * @return The properties in declaration order and the loader, or why the struct can't be used (unsupported field type, over the material budget, duplicate reference).
 */
[[nodiscard]] auto shader_properties_from_reflection(const graphics::shader_compiler::module_reflection& reflection) -> std::expected<reflected_properties, std::string>;

/** @brief Everything a `.slang` shader asset learns from parsing its file. */
struct shader_reflection_result {
  shader_kind kind{shader_kind::library};
  bool has_depth_entry_points{false};
  std::vector<shader_property> properties{};
  std::string material_loader{};
  std::string error{}; // empty unless the file doesn't parse or its properties struct can't be used
  std::vector<std::filesystem::path> dependencies{}; // normalized, for matching an edited #include to the shaders it affects
}; // struct shader_reflection_result

/**
 * @brief Parses a `.slang` shader and works out its kind, depth entry points, material properties and #include dependencies.
 *
 * Thread-safe: shader_compiler serializes Slang access, so the asset loader thread can call it.
 *
 * @param path The shader file.
 *
 * @return The result; a file that doesn't parse comes back as a library with the error set.
 */
[[nodiscard]] auto reflect_shader(const std::filesystem::path& path) -> shader_reflection_result;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_REFLECTION_HPP_
