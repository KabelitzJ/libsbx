// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <fstream>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

inline constexpr auto material_magic = utility::fourcc_v<"SBMT">; // 'SBMT'

// Texture slots are variable-length path strings (assets-directory-relative, empty = none), not
// fixed uuid64s -- see material_description's doc comment for why. name and the five slot strings
// follow this header back to back, each preceded by nothing (lengths are all up front here) in
// the fixed order: name, albedo, normal, metallic_roughness, occlusion, emissive.
struct material_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::float_t base_color_factor[4];
  std::float_t emissive_factor[3];
  std::float_t metallic_factor;
  std::float_t roughness_factor;
  std::uint32_t alpha_mode;
  std::uint32_t shading_model;
  std::float_t alpha_cutoff;
  std::uint32_t is_double_sided;
  std::float_t normal_scale;
  std::float_t occlusion_strength;
  std::float_t emissive_strength;
  std::float_t ior;
  std::float_t uv_tiling[2];
  std::float_t uv_offset[2];
  std::uint32_t name_length;
  std::uint32_t albedo_path_length;
  std::uint32_t normal_path_length;
  std::uint32_t metallic_roughness_path_length;
  std::uint32_t occlusion_path_length;
  std::uint32_t emissive_path_length;
}; // struct material_file_header

auto asset_cooker::resolve_cooked_material(const math::uuid& id) -> std::optional<material_description> {
  const auto cooked = cooked_path(id, ".sbxmat");

  if (!std::filesystem::exists(cooked)) {
    return std::nullopt;
  }

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    utility::logger<"assets">::warn("Could not open cooked material '{}'", cooked.generic_string());
    return std::nullopt;
  }

  auto header = material_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != material_magic || header.version != material_cook_version) {
    utility::logger<"assets">::warn("Invalid cooked material '{}'", cooked.generic_string());
    return std::nullopt;
  }

  const auto read_string = [&in](std::uint32_t length) -> std::optional<std::string> {
    auto value = std::string(length, '\0');

    if (length > 0u) {
      in.read(value.data(), static_cast<std::streamsize>(length));

      if (!in) {
        return std::nullopt;
      }
    }

    return value;
  };

  const auto name = read_string(header.name_length);
  const auto albedo = read_string(header.albedo_path_length);
  const auto normal = read_string(header.normal_path_length);
  const auto metallic_roughness = read_string(header.metallic_roughness_path_length);
  const auto occlusion = read_string(header.occlusion_path_length);
  const auto emissive = read_string(header.emissive_path_length);

  if (!name || !albedo || !normal || !metallic_roughness || !occlusion || !emissive) {
    return std::nullopt;
  }

  auto description = material_description{};
  description.name = name->empty() ? std::string{"material"} : *name;
  description.base_color_factor = math::color{header.base_color_factor[0], header.base_color_factor[1], header.base_color_factor[2], header.base_color_factor[3]};
  description.emissive_factor = math::vector3{header.emissive_factor[0], header.emissive_factor[1], header.emissive_factor[2]};
  description.metallic_factor = header.metallic_factor;
  description.roughness_factor = header.roughness_factor;
  description.alpha = static_cast<alpha_mode>(header.alpha_mode);
  description.shading = static_cast<shading_model>(header.shading_model);
  description.alpha_cutoff = header.alpha_cutoff;
  description.is_double_sided = header.is_double_sided != 0u;
  description.normal_scale = header.normal_scale;
  description.occlusion_strength = header.occlusion_strength;
  description.emissive_strength = header.emissive_strength;
  description.ior = header.ior;
  description.uv_tiling = math::vector2{header.uv_tiling[0], header.uv_tiling[1]};
  description.uv_offset = math::vector2{header.uv_offset[0], header.uv_offset[1]};
  description.albedo = *albedo;
  description.normal = *normal;
  description.metallic_roughness = *metallic_roughness;
  description.occlusion = *occlusion;
  description.emissive = *emissive;

  return description;
}

auto asset_cooker::derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid {
  // splitmix64 over (mesh uuid, index) — deterministic so re-cooking is stable.
  auto x = mesh.value() ^ (0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(index) + 1ull));
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::parse_material_file(const std::filesystem::path& source) -> std::optional<material_description> {
  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(source.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse material '{}' ({})", source.generic_string(), exception.what());
    return std::nullopt;
  }

  auto description = material_description{};

  if (root["name"]) description.name = root["name"].as<std::string>();
  if (root["base_color_factor"]) description.base_color_factor = root["base_color_factor"].as<math::color>();
  if (root["emissive_factor"]) description.emissive_factor = root["emissive_factor"].as<math::vector3>();
  if (root["metallic_factor"]) description.metallic_factor = root["metallic_factor"].as<std::float_t>();
  if (root["roughness_factor"]) description.roughness_factor = root["roughness_factor"].as<std::float_t>();
  if (root["alpha_mode"]) {
    const auto mode = root["alpha_mode"].as<std::string>();
    description.alpha = (mode == "blend") ? alpha_mode::blend : (mode == "mask") ? alpha_mode::mask : alpha_mode::opaque;
  }
  if (root["shading_model"]) {
    const auto model = root["shading_model"].as<std::string>();
    description.shading = (model == "unlit") ? shading_model::unlit : shading_model::pbr;
  }
  if (root["alpha_cutoff"]) description.alpha_cutoff = root["alpha_cutoff"].as<std::float_t>();
  if (root["is_double_sided"]) description.is_double_sided = root["is_double_sided"].as<bool>();
  if (root["casts_shadow"]) description.casts_shadow = root["casts_shadow"].as<bool>();
  if (root["receives_shadow"]) description.receives_shadow = root["receives_shadow"].as<bool>();
  if (root["normal_scale"]) description.normal_scale = root["normal_scale"].as<std::float_t>();
  if (root["occlusion_strength"]) description.occlusion_strength = root["occlusion_strength"].as<std::float_t>();
  if (root["emissive_strength"]) description.emissive_strength = root["emissive_strength"].as<std::float_t>();
  if (root["ior"]) description.ior = root["ior"].as<std::float_t>();
  if (root["uv_tiling"]) description.uv_tiling = root["uv_tiling"].as<math::vector2>();
  if (root["uv_offset"]) description.uv_offset = root["uv_offset"].as<math::vector2>();

  const auto path_slot = [&](const char* key) -> std::string {
    if (const auto node = root[key]) {
      return node.as<std::string>();
    }
    return {};
  };

  description.albedo = path_slot("albedo");
  description.normal = path_slot("normal");
  description.metallic_roughness = path_slot("metallic_roughness");
  description.occlusion = path_slot("occlusion");
  description.emissive = path_slot("emissive");

  description.shader_graph = path_slot("shader_graph");

  if (const auto generic_params = root["generic_params"]) {
    for (auto i = std::size_t{0u}; i < generic_params.size() && i < description.generic_params.size(); ++i) {
      description.generic_params[i] = generic_params[i].as<math::vector4>();
    }
  }

  if (const auto generic_textures = root["generic_textures"]) {
    for (auto i = std::size_t{0u}; i < generic_textures.size() && i < description.generic_texture_paths.size(); ++i) {
      description.generic_texture_paths[i] = generic_textures[i].as<std::string>();
    }
  }

  return description;
}

auto asset_cooker::write_cooked_material(const math::uuid& id, const material_description& description) -> bool {
  const auto cooked = cooked_path(id, ".sbxmat");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write material '{}'", cooked.generic_string());
    return false;
  }

  auto header = material_file_header{};
  header.magic = material_magic;
  header.version = material_cook_version;
  header.base_color_factor[0] = description.base_color_factor.r();
  header.base_color_factor[1] = description.base_color_factor.g();
  header.base_color_factor[2] = description.base_color_factor.b();
  header.base_color_factor[3] = description.base_color_factor.a();
  header.emissive_factor[0] = description.emissive_factor.x();
  header.emissive_factor[1] = description.emissive_factor.y();
  header.emissive_factor[2] = description.emissive_factor.z();
  header.metallic_factor = description.metallic_factor;
  header.roughness_factor = description.roughness_factor;
  header.alpha_mode = static_cast<std::uint32_t>(description.alpha);
  header.shading_model = static_cast<std::uint32_t>(description.shading);
  header.alpha_cutoff = description.alpha_cutoff;
  header.is_double_sided = description.is_double_sided ? 1u : 0u;
  header.normal_scale = description.normal_scale;
  header.occlusion_strength = description.occlusion_strength;
  header.emissive_strength = description.emissive_strength;
  header.ior = description.ior;
  header.uv_tiling[0] = description.uv_tiling.x();
  header.uv_tiling[1] = description.uv_tiling.y();
  header.uv_offset[0] = description.uv_offset.x();
  header.uv_offset[1] = description.uv_offset.y();
  header.name_length = static_cast<std::uint32_t>(description.name.size());
  header.albedo_path_length = static_cast<std::uint32_t>(description.albedo.size());
  header.normal_path_length = static_cast<std::uint32_t>(description.normal.size());
  header.metallic_roughness_path_length = static_cast<std::uint32_t>(description.metallic_roughness.size());
  header.occlusion_path_length = static_cast<std::uint32_t>(description.occlusion.size());
  header.emissive_path_length = static_cast<std::uint32_t>(description.emissive.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(description.name.data(), static_cast<std::streamsize>(description.name.size()));
  out.write(description.albedo.data(), static_cast<std::streamsize>(description.albedo.size()));
  out.write(description.normal.data(), static_cast<std::streamsize>(description.normal.size()));
  out.write(description.metallic_roughness.data(), static_cast<std::streamsize>(description.metallic_roughness.size()));
  out.write(description.occlusion.data(), static_cast<std::streamsize>(description.occlusion.size()));
  out.write(description.emissive.data(), static_cast<std::streamsize>(description.emissive.size()));

  utility::logger<"assets">::debug("Wrote generated material '{}'", cooked.generic_string());

  return true;
}

auto asset_cooker::_cook_material(const math::uuid& id, const material_description& description) -> bool {
  const auto cooked = cooked_path(id, ".sbxmat");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write material '{}'", cooked.generic_string());
    return false;
  }

  auto header = material_file_header{};
  header.magic = material_magic;
  header.version = material_cook_version;
  header.base_color_factor[0] = description.base_color_factor.r();
  header.base_color_factor[1] = description.base_color_factor.g();
  header.base_color_factor[2] = description.base_color_factor.b();
  header.base_color_factor[3] = description.base_color_factor.a();
  header.emissive_factor[0] = description.emissive_factor.x();
  header.emissive_factor[1] = description.emissive_factor.y();
  header.emissive_factor[2] = description.emissive_factor.z();
  header.metallic_factor = description.metallic_factor;
  header.roughness_factor = description.roughness_factor;
  header.alpha_mode = static_cast<std::uint32_t>(description.alpha);
  header.shading_model = static_cast<std::uint32_t>(description.shading);
  header.alpha_cutoff = description.alpha_cutoff;
  header.is_double_sided = description.is_double_sided ? 1u : 0u;
  header.normal_scale = description.normal_scale;
  header.occlusion_strength = description.occlusion_strength;
  header.emissive_strength = description.emissive_strength;
  header.ior = description.ior;
  header.uv_tiling[0] = description.uv_tiling.x();
  header.uv_tiling[1] = description.uv_tiling.y();
  header.uv_offset[0] = description.uv_offset.x();
  header.uv_offset[1] = description.uv_offset.y();
  header.name_length = static_cast<std::uint32_t>(description.name.size());
  header.albedo_path_length = static_cast<std::uint32_t>(description.albedo.size());
  header.normal_path_length = static_cast<std::uint32_t>(description.normal.size());
  header.metallic_roughness_path_length = static_cast<std::uint32_t>(description.metallic_roughness.size());
  header.occlusion_path_length = static_cast<std::uint32_t>(description.occlusion.size());
  header.emissive_path_length = static_cast<std::uint32_t>(description.emissive.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(description.name.data(), static_cast<std::streamsize>(description.name.size()));
  out.write(description.albedo.data(), static_cast<std::streamsize>(description.albedo.size()));
  out.write(description.normal.data(), static_cast<std::streamsize>(description.normal.size()));
  out.write(description.metallic_roughness.data(), static_cast<std::streamsize>(description.metallic_roughness.size()));
  out.write(description.occlusion.data(), static_cast<std::streamsize>(description.occlusion.size()));
  out.write(description.emissive.data(), static_cast<std::streamsize>(description.emissive.size()));

  return true;
}


} // namespace sbx::assets
