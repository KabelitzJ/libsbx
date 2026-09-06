// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_residency.hpp>

#include <fstream>
#include <type_traits>
#include <utility>
#include <variant>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/timer.hpp>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

namespace sbx::assets {

inline constexpr auto material_flag_masked = std::uint32_t{1u << 0u};
inline constexpr auto material_flag_receives_shadow = std::uint32_t{1u << 1u};

struct material_data {
  math::vector4 base_color_factor;
  math::vector4 emissive_factor;
  std::uint32_t albedo_index;
  std::uint32_t normal_index;
  std::uint32_t metallic_roughness_index;
  std::uint32_t occlusion_index;
  std::uint32_t emissive_index;
  std::float_t metallic_factor;
  std::float_t roughness_factor;
  std::float_t alpha_cutoff;
  std::uint32_t flags;
  std::float_t normal_scale;
  std::float_t occlusion_strength;
  std::float_t emissive_strength;
  std::float_t ior;
}; // struct material_data

// Strips characters a filename can't contain, for turning a gltf material's (freeform) name into
// a safe file name when extracting it.
static auto sanitize_file_name(std::string name) -> std::string {
  for (auto& character : name) {
    if (character == '/' || character == '\\' || character == ':' || character == '*' || character == '?' || character == '"' || character == '<' || character == '>' || character == '|') {
      character = '_';
    }
  }

  return name;
}

asset_residency::asset_residency(asset_cooker& cooker, ibl_baker& baker)
: _cooker{cooker},
  _ibl{baker} {
  _white = _create_default_texture({255u, 255u, 255u, 255u});
  _normal = _create_default_texture({128u, 128u, 255u, 255u}); // (0,0,1) tangent-space normal
  _black = _create_default_texture({0u, 0u, 0u, 255u});
  _magenta = _create_default_texture({255u, 0u, 255u, 255u});   // load-error marker
}

auto asset_residency::load_texture(const math::uuid& id, graphics::format format) -> texture_handle {
  const auto is_srgb = (format == graphics::format::r8g8b8a8_srgb);

  const auto key = fmt::format("{}:{}", id.value(), (is_srgb ? "#srgb" : "#linear"));

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _textures.find(key); entry != _textures.end()) {
      return texture_handle{entry->second};
    }
  }

  auto data = _cooker.resolve_texture(id);

  if (!data) {
    return texture_handle{};
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};

    _textures.emplace(key, record);
    _pending_textures.push_back(pending_texture_upload{index, std::move(data->pixels), data->width, data->height, format});
  }

  return texture_handle{record};
}

auto asset_residency::load_texture(const std::filesystem::path& path, graphics::format format) -> texture_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_texture(_cooker.import(assets_directory / path), format);
}

auto asset_residency::load_mesh(const math::uuid& id, const mesh_import_options& options) -> mesh_handle {
  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _meshes.find(id); entry != _meshes.end()) {
      return mesh_handle{entry->second};
    }
  }

  auto data = _cooker.resolve_mesh(id, options, [this](const material_description& description, const std::filesystem::path& relative_source) {
    return _extract_gltf_material(description, relative_source);
  });

  if (!data) {
    return mesh_handle{};
  }

  auto fallback_material = material_handle{};

  auto submeshes = std::vector<mesh::submesh>{};
  submeshes.reserve(data->submeshes.size());

  for (const auto& cooked_submesh : data->submeshes) {
    auto material = material_handle{};

    if (cooked_submesh.material != math::uuid::nil()) {
      material = load_material(cooked_submesh.material);
    }

    if (!material.is_valid()) {
      if (!fallback_material.is_valid()) {
        fallback_material = create_material(material::create_info{ .albedo = _magenta });
      }
      material = fallback_material;
    }

    auto lods = std::vector<mesh::lod_level>{};
    lods.reserve(cooked_submesh.lods.size());

    for (const auto& lod : cooked_submesh.lods) {
      lods.push_back(mesh::lod_level{lod.index_offset, lod.index_count, lod.error});
    }

    submeshes.push_back(mesh::submesh{cooked_submesh.index_offset, cooked_submesh.index_count, cooked_submesh.bounds, material, std::move(lods)});
  }

  const auto vertex_count = data->vertices.size();
  const auto index_count = data->indices.size();
  const auto submesh_count = submeshes.size();

  auto record = std::make_shared<mesh>(std::move(submeshes), data->bounds, static_cast<std::uint32_t>(vertex_count));
  record->_id = id;

  if (!data->skin_vertices.empty()) {
    auto skeleton_handle_value = load_skeleton(data->skeleton);

    auto animation_clip_handles = std::vector<animation_clip_handle>{};
    animation_clip_handles.reserve(data->animation_clips.size());

    for (const auto& clip_id : data->animation_clips) {
      animation_clip_handles.push_back(load_animation_clip(clip_id));
    }

    record->_set_skeletal_data(skeleton_handle_value, std::move(animation_clip_handles));
  }

  {
    auto lock = std::lock_guard{_mutex};

    _meshes.emplace(id, record);
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(data->vertices), std::move(data->indices), std::move(data->skin_vertices)});
  }

  if (record->skeleton().is_valid()) {
    utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes, {} joints, {} animation clips", _cooker.path_of(id).generic_string(), vertex_count, index_count, submesh_count, record->skeleton()->joints().size(), record->animation_clips().size());
  } else {
    utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes", _cooker.path_of(id).generic_string(), vertex_count, index_count, submesh_count);
  }

  return mesh_handle{record};
}

auto asset_residency::load_skeleton(const math::uuid& id) -> skeleton_handle {
  if (id == math::uuid::nil()) {
    return skeleton_handle{};
  }

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _skeletons.find(id); entry != _skeletons.end()) {
      return skeleton_handle{entry->second};
    }
  }

  auto joints = _cooker.resolve_skeleton(id);

  if (!joints) {
    utility::logger<"assets">::warn("Could not load skeleton {}", id);
    return skeleton_handle{};
  }

  auto record = std::make_shared<skeleton>(std::move(*joints));
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _skeletons.emplace(id, record);
  }

  return skeleton_handle{record};
}

auto asset_residency::load_animation_clip(const math::uuid& id) -> animation_clip_handle {
  if (id == math::uuid::nil()) {
    return animation_clip_handle{};
  }

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _animation_clips.find(id); entry != _animation_clips.end()) {
      return animation_clip_handle{entry->second};
    }
  }

  auto data = _cooker.resolve_animation_clip(id);

  if (!data) {
    utility::logger<"assets">::warn("Could not load animation clip {}", id);
    return animation_clip_handle{};
  }

  auto record = std::make_shared<animation_clip>(std::move(data->name), data->duration, std::move(data->channels));
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _animation_clips.emplace(id, record);
  }

  return animation_clip_handle{record};
}

auto asset_residency::load_mesh(const std::filesystem::path& path, const mesh_import_options& options) -> mesh_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_mesh(_cooker.import(assets_directory / path), options);
}

auto asset_residency::load_material(const math::uuid& id) -> material_handle {
  _cooker.ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _material_files.find(id); entry != _material_files.end()) {
      return material_handle{entry->second};
    }
  }

  const auto source_path = _cooker.path_of(id);

  if (!source_path.empty() && source_path.extension() == ".material") {
    const auto& path = source_path;

    auto root = YAML::Node{};
    try {
      root = YAML::LoadFile(path.string());
    } catch (const std::exception& exception) {
      utility::logger<"assets">::warn("Could not parse material '{}' ({})", path.generic_string(), exception.what());
      return material_handle{};
    }

    auto info = material::create_info{};

    if (root["name"]) info.name = root["name"].as<std::string>();
    if (root["base_color_factor"]) info.base_color_factor = root["base_color_factor"].as<math::color>();
    if (root["emissive_factor"]) info.emissive_factor = root["emissive_factor"].as<math::vector3>();
    if (root["metallic_factor"]) info.metallic_factor = root["metallic_factor"].as<std::float_t>();
    if (root["roughness_factor"]) info.roughness_factor = root["roughness_factor"].as<std::float_t>();
    if (root["alpha_mode"]) {
      const auto mode = root["alpha_mode"].as<std::string>();
      info.alpha = (mode == "blend") ? alpha_mode::blend : (mode == "mask") ? alpha_mode::mask : alpha_mode::opaque;
    }
    if (root["alpha_cutoff"]) info.alpha_cutoff = root["alpha_cutoff"].as<std::float_t>();
    if (root["is_double_sided"]) info.is_double_sided = root["is_double_sided"].as<bool>();
    if (root["casts_shadow"]) info.casts_shadow = root["casts_shadow"].as<bool>();
    if (root["receives_shadow"]) info.receives_shadow = root["receives_shadow"].as<bool>();
    if (root["normal_scale"]) info.normal_scale = root["normal_scale"].as<std::float_t>();
    if (root["occlusion_strength"]) info.occlusion_strength = root["occlusion_strength"].as<std::float_t>();
    if (root["emissive_strength"]) info.emissive_strength = root["emissive_strength"].as<std::float_t>();
    if (root["ior"]) info.ior = root["ior"].as<std::float_t>();

    const auto load_slot = [&](const char* key, graphics::format format) -> texture_handle {
      if (const auto node = root[key]) {
        return load_texture(std::filesystem::path{node.as<std::string>()}, format);
      }
      return texture_handle{};
    };

    info.albedo = load_slot("albedo", graphics::format::r8g8b8a8_srgb);
    info.normal = load_slot("normal", graphics::format::r8g8b8a8_unorm);
    info.metallic_roughness = load_slot("metallic_roughness", graphics::format::r8g8b8a8_unorm);
    info.occlusion = load_slot("occlusion", graphics::format::r8g8b8a8_unorm);
    info.emissive = load_slot("emissive", graphics::format::r8g8b8a8_srgb);

    auto record = std::make_shared<material>(info);
    record->_id = id;

    auto handle = _register_material(record);

    {
      auto lock = std::lock_guard{_mutex};
      _material_files.emplace(id, record);
    }

    utility::logger<"assets">::info("Loaded material '{}'", path.generic_string());

    return handle;
  }

  // Otherwise a cooked material extracted from a mesh import.
  if (const auto description = _cooker.resolve_cooked_material(id)) {
    auto info = material::create_info{};
    info.name = description->name;
    info.base_color_factor = description->base_color_factor;
    info.emissive_factor = description->emissive_factor;
    info.metallic_factor = description->metallic_factor;
    info.roughness_factor = description->roughness_factor;
    info.alpha = description->alpha;
    info.alpha_cutoff = description->alpha_cutoff;
    info.is_double_sided = description->is_double_sided;
    info.normal_scale = description->normal_scale;
    info.occlusion_strength = description->occlusion_strength;
    info.emissive_strength = description->emissive_strength;
    info.ior = description->ior;

    const auto load_slot = [&](const math::uuid& uuid, graphics::format format) -> texture_handle {
      return (uuid == math::uuid::nil()) ? texture_handle{} : load_texture(uuid, format);
    };

    info.albedo = load_slot(description->albedo, graphics::format::r8g8b8a8_srgb);
    info.normal = load_slot(description->normal, graphics::format::r8g8b8a8_unorm);
    info.metallic_roughness = load_slot(description->metallic_roughness, graphics::format::r8g8b8a8_unorm);
    info.occlusion = load_slot(description->occlusion, graphics::format::r8g8b8a8_unorm);
    info.emissive = load_slot(description->emissive, graphics::format::r8g8b8a8_srgb);

    auto record = std::make_shared<material>(info);
    record->_id = id;

    auto handle = _register_material(record);

    {
      auto lock = std::lock_guard{_mutex};
      _material_files.emplace(id, record);
    }

    return handle;
  }

  utility::logger<"assets">::warn("Unknown material uuid {}", id);

  return material_handle{};
}

auto asset_residency::load_material(const std::filesystem::path& path) -> material_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_material(_cooker.import(assets_directory / path));
}

auto asset_residency::create_material(const material::create_info& create_info) -> material_handle {
  return _register_material(std::make_shared<material>(create_info));
}

auto asset_residency::update_material(material_handle& material, const material::create_info& create_info) -> void {
  if (!material.is_valid()) {
    return;
  }

  material->_base_color_factor = create_info.base_color_factor;
  material->_emissive_factor = create_info.emissive_factor;
  material->_metallic_factor = create_info.metallic_factor;
  material->_roughness_factor = create_info.roughness_factor;
  material->_alpha = create_info.alpha;
  material->_alpha_cutoff = create_info.alpha_cutoff;
  material->_is_double_sided = create_info.is_double_sided;
  material->_casts_shadow = create_info.casts_shadow;
  material->_receives_shadow = create_info.receives_shadow;
  material->_normal_scale = create_info.normal_scale;
  material->_occlusion_strength = create_info.occlusion_strength;
  material->_emissive_strength = create_info.emissive_strength;
  material->_ior = create_info.ior;
  material->_albedo = create_info.albedo;
  material->_normal = create_info.normal;
  material->_metallic_roughness = create_info.metallic_roughness;
  material->_occlusion = create_info.occlusion;
  material->_emissive = create_info.emissive;
  material->_name = create_info.name;

  // Re-queue the upload: _register_material only queues one at creation time, so without this an
  // in-place edit updates the CPU object but the renderer keeps reading the stale uploaded data.
  auto lock = std::lock_guard{_mutex};

  if (material->index() < _materials.size()) {
    _pending_materials.push_back(pending_material_upload{_materials[material->index()]});
  }
}

auto asset_residency::save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!material.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid material to '{}'", resolved_path.generic_string());
    return math::uuid::nil();
  }

  const auto path_of = [this](const texture_handle& texture) -> std::optional<std::string> {
    if (!texture.is_valid()) {
      return std::nullopt;
    }

    const auto absolute = _cooker.path_of(texture->id());

    if (absolute.empty()) {
      return std::nullopt; // default/procedural texture (nil uuid) — omit the slot
    }

    // absolute is stored fully resolved; the slot needs to hold the assets-relative form (that's
    // what load_material's own reader passes straight into load_texture(path, ...)).
    return _cooker.relative(absolute).generic_string();
  };

  auto node = YAML::Node{};

  node["name"] = material->name();
  node["base_color_factor"] = material->base_color_factor();
  node["emissive_factor"] = material->emissive_factor();
  node["metallic_factor"] = material->metallic_factor();
  node["roughness_factor"] = material->roughness_factor();
  node["alpha_mode"] = (material->alpha() == alpha_mode::blend) ? "blend" : (material->alpha() == alpha_mode::mask) ? "mask" : "opaque";
  node["alpha_cutoff"] = material->alpha_cutoff();
  node["is_double_sided"] = material->is_double_sided();
  node["casts_shadow"] = material->casts_shadow();
  node["receives_shadow"] = material->receives_shadow();
  node["normal_scale"] = material->normal_scale();
  node["occlusion_strength"] = material->occlusion_strength();
  node["emissive_strength"] = material->emissive_strength();
  node["ior"] = material->ior();

  if (const auto slot = path_of(material->albedo())) {
    node["albedo"] = *slot;
  }

  if (const auto slot = path_of(material->normal())) {
    node["normal"] = *slot;
  }

  if (const auto slot = path_of(material->metallic_roughness())) {
    node["metallic_roughness"] = *slot;
  }

  if (const auto slot = path_of(material->occlusion())) {
    node["occlusion"] = *slot;
  }

  if (const auto slot = path_of(material->emissive())) {
    node["emissive"] = *slot;
  }

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = _cooker.import(resolved_path); // register + create the .meta so it's a first-class asset

  // import() is idempotent (returns the existing uuid from .meta on a re-save), so this always
  // stamps the right id — including a create_material()'d material's first save (nil id otherwise).
  material->_id = id;

  utility::logger<"assets">::info("Saved material '{}'", resolved_path.generic_string());

  return id;
}

auto asset_residency::load_particle_effect(const math::uuid& id) -> particle_effect_handle {
  _cooker.ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _particle_effect_files.find(id); entry != _particle_effect_files.end()) {
      return particle_effect_handle{entry->second};
    }
  }

  const auto source_path = _cooker.path_of(id);

  if (source_path.empty() || source_path.extension() != ".particle_effect") {
    utility::logger<"assets">::warn("Unknown particle_effect uuid {}", id);
    return particle_effect_handle{};
  }

  const auto& path = source_path;

  auto root = YAML::Node{};
  try {
    root = YAML::LoadFile(path.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse particle_effect '{}' ({})", path.generic_string(), exception.what());
    return particle_effect_handle{};
  }

  auto info = particle_effect::create_info{};

  if (root["name"]) info.name = root["name"].as<std::string>();

  const auto load_curve = [](const YAML::Node& node) -> curve {
    auto result = curve{};

    if (!node) {
      return result;
    }

    for (const auto key_node : node) {
      if (result.keys.is_full()) {
        break;
      }

      auto key = curve_key{};

      if (key_node["time"]) key.time = key_node["time"].as<std::float_t>();
      if (key_node["value"]) key.value = key_node["value"].as<std::float_t>();

      result.keys.push_back(key);
    }

    return result;
  };

  const auto load_gradient = [](const YAML::Node& node) -> gradient {
    auto result = gradient{};

    if (!node) {
      return result;
    }

    if (const auto color_keys_node = node["color_keys"]) {
      for (const auto key_node : color_keys_node) {
        if (result.color_keys.is_full()) {
          break;
        }

        auto key = gradient_color_key{};

        if (key_node["time"]) key.time = key_node["time"].as<std::float_t>();
        if (key_node["color"]) key.color = key_node["color"].as<math::color>();

        result.color_keys.push_back(key);
      }
    }

    if (const auto alpha_keys_node = node["alpha_keys"]) {
      for (const auto key_node : alpha_keys_node) {
        if (result.alpha_keys.is_full()) {
          break;
        }

        auto key = gradient_alpha_key{};

        if (key_node["time"]) key.time = key_node["time"].as<std::float_t>();
        if (key_node["alpha"]) key.alpha = key_node["alpha"].as<std::float_t>();

        result.alpha_keys.push_back(key);
      }
    }

    return result;
  };

  if (const auto emitters = root["emitters"]) {
    info.emitters.reserve(emitters.size());

    for (const auto emitter_node : emitters) {
      auto emitter = particle_emitter{};

      if (emitter_node["name"]) emitter.name = emitter_node["name"].as<std::string>();

      if (emitter_node["blend_mode"]) {
        const auto mode = emitter_node["blend_mode"].as<std::string>();
        emitter.blend_mode = (mode == "alpha_blend") ? emitter_blend_mode::alpha_blend : emitter_blend_mode::additive;
      }

      if (emitter_node["simulation_mode"]) {
        const auto mode = emitter_node["simulation_mode"].as<std::string>();
        emitter.simulation_mode = (mode == "gpu") ? particle_simulation_mode::gpu : particle_simulation_mode::cpu;
      }

      if (emitter_node["emission_rate"]) emitter.emission_rate = emitter_node["emission_rate"].as<std::float_t>();
      if (emitter_node["burst_count"]) emitter.burst_count = emitter_node["burst_count"].as<std::uint32_t>();

      if (emitter_node["shape"]) {
        const auto shape = emitter_node["shape"].as<std::string>();
        emitter.shape = (shape == "sphere") ? emitter_shape::sphere : (shape == "box") ? emitter_shape::box : (shape == "cone") ? emitter_shape::cone : emitter_shape::point;
      }

      if (emitter_node["shape_extents"]) emitter.shape_extents = emitter_node["shape_extents"].as<math::vector3>();

      if (const auto cone_node = emitter_node["cone"]) {
        if (cone_node["angle_degrees"]) emitter.cone.angle = math::degree{cone_node["angle_degrees"].as<std::float_t>()};
        if (cone_node["radius"]) emitter.cone.radius = cone_node["radius"].as<std::float_t>();
        if (cone_node["emit_from_volume"]) emitter.cone.emit_from_volume = cone_node["emit_from_volume"].as<std::float_t>();
      }

      if (emitter_node["velocity_min"]) emitter.velocity_min = emitter_node["velocity_min"].as<math::vector3>();
      if (emitter_node["velocity_max"]) emitter.velocity_max = emitter_node["velocity_max"].as<math::vector3>();
      if (emitter_node["lifetime_min"]) emitter.lifetime_min = emitter_node["lifetime_min"].as<std::float_t>();
      if (emitter_node["lifetime_max"]) emitter.lifetime_max = emitter_node["lifetime_max"].as<std::float_t>();
      if (emitter_node["start_color"]) emitter.start_color = emitter_node["start_color"].as<math::color>();
      if (emitter_node["end_color"]) emitter.end_color = emitter_node["end_color"].as<math::color>();
      if (emitter_node["color_over_lifetime"]) emitter.color_over_lifetime = load_gradient(emitter_node["color_over_lifetime"]);
      if (emitter_node["size_min"]) emitter.size_min = emitter_node["size_min"].as<std::float_t>();
      if (emitter_node["size_max"]) emitter.size_max = emitter_node["size_max"].as<std::float_t>();
      if (emitter_node["size_over_lifetime"]) emitter.size_over_lifetime = load_curve(emitter_node["size_over_lifetime"]);
      if (emitter_node["rotation_min"]) emitter.rotation_min = emitter_node["rotation_min"].as<std::float_t>();
      if (emitter_node["rotation_max"]) emitter.rotation_max = emitter_node["rotation_max"].as<std::float_t>();
      if (emitter_node["rotation_over_lifetime"]) emitter.rotation_over_lifetime = load_curve(emitter_node["rotation_over_lifetime"]);

      if (const auto velocity_curve_node = emitter_node["velocity_over_lifetime"]) {
        emitter.velocity_over_lifetime.x = load_curve(velocity_curve_node["x"]);
        emitter.velocity_over_lifetime.y = load_curve(velocity_curve_node["y"]);
        emitter.velocity_over_lifetime.z = load_curve(velocity_curve_node["z"]);
      }

      if (emitter_node["force_over_lifetime_min"]) emitter.force_over_lifetime_min = emitter_node["force_over_lifetime_min"].as<math::vector3>();
      if (emitter_node["force_over_lifetime_max"]) emitter.force_over_lifetime_max = emitter_node["force_over_lifetime_max"].as<math::vector3>();

      if (emitter_node["gravity"]) emitter.gravity = emitter_node["gravity"].as<std::float_t>();
      if (emitter_node["drag"]) emitter.drag = emitter_node["drag"].as<std::float_t>();

      if (emitter_node["texture"]) {
        emitter.texture = load_texture(std::filesystem::path{emitter_node["texture"].as<std::string>()}, graphics::format::r8g8b8a8_srgb);
      }

      if (emitter_node["render_mode"]) {
        emitter.render_mode = (emitter_node["render_mode"].as<std::string>() == "mesh") ? particle_render_mode::mesh : particle_render_mode::billboard;
      }

      if (emitter_node["render_mesh"]) {
        emitter.render_mesh = load_mesh(std::filesystem::path{emitter_node["render_mesh"].as<std::string>()});
      }

      if (emitter_node["render_material"]) {
        emitter.render_material = load_material(std::filesystem::path{emitter_node["render_material"].as<std::string>()});
      }

      if (const auto collision_node = emitter_node["collision"]) {
        auto& collision = emitter.collision;

        if (collision_node["mode"]) {
          const auto mode = collision_node["mode"].as<std::string>();
          collision.mode = (mode == "planes") ? particle_collision_mode::planes : (mode == "world") ? particle_collision_mode::world : particle_collision_mode::none;
        }

        if (collision_node["bounce"]) collision.bounce = collision_node["bounce"].as<std::float_t>();
        if (collision_node["lifetime_loss"]) collision.lifetime_loss = collision_node["lifetime_loss"].as<std::float_t>();
        if (collision_node["dampen"]) collision.dampen = collision_node["dampen"].as<std::float_t>();
        if (collision_node["radius_scale"]) collision.radius_scale = collision_node["radius_scale"].as<std::float_t>();
        if (collision_node["max_collisions_per_particle"]) collision.max_collisions_per_particle = collision_node["max_collisions_per_particle"].as<std::uint32_t>();

        if (const auto planes_node = collision_node["planes"]) {
          for (const auto plane_node : planes_node) {
            if (collision.planes.size() >= collision_max_planes) {
              break;
            }

            auto plane = collision_plane{};

            if (plane_node["normal"]) plane.normal = plane_node["normal"].as<math::vector3>();
            if (plane_node["distance"]) plane.distance = plane_node["distance"].as<std::float_t>();

            collision.planes.push_back(plane);
          }
        }
      }

      if (const auto sub_emitters_node = emitter_node["sub_emitters"]) {
        for (const auto binding_node : sub_emitters_node) {
          auto binding = sub_emitter_binding{};

          if (binding_node["event"]) {
            const auto event = binding_node["event"].as<std::string>();
            binding.event = (event == "death") ? sub_emitter_event::death : (event == "collision") ? sub_emitter_event::collision : sub_emitter_event::birth;
          }

          if (binding_node["effect"]) {
            binding.effect = load_particle_effect(std::filesystem::path{binding_node["effect"].as<std::string>()});
          }

          if (binding_node["probability"]) binding.probability = binding_node["probability"].as<std::float_t>();
          if (binding_node["inherit_velocity"]) binding.inherit_velocity = binding_node["inherit_velocity"].as<bool>();

          emitter.sub_emitters.push_back(binding);
        }
      }

      if (const auto trail_node = emitter_node["trail"]) {
        auto& trail = emitter.trail;

        if (trail_node["enabled"]) trail.enabled = trail_node["enabled"].as<bool>();
        if (trail_node["min_vertex_distance"]) trail.min_vertex_distance = trail_node["min_vertex_distance"].as<std::float_t>();
        if (trail_node["lifetime"]) trail.lifetime = trail_node["lifetime"].as<std::float_t>();
        if (trail_node["width"]) trail.width = trail_node["width"].as<std::float_t>();
        if (trail_node["color_over_trail"]) trail.color_over_trail = load_gradient(trail_node["color_over_trail"]);
        if (trail_node["die_with_particle"]) trail.die_with_particle = trail_node["die_with_particle"].as<bool>();
      }

      info.emitters.push_back(emitter);
    }
  }

  auto record = std::make_shared<particle_effect>(info);
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _particle_effect_files.emplace(id, record);
  }

  utility::logger<"assets">::info("Loaded particle_effect '{}'", path.generic_string());

  return particle_effect_handle{record};
}

auto asset_residency::load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_particle_effect(_cooker.import(assets_directory / path));
}

auto asset_residency::create_particle_effect(const particle_effect::create_info& create_info) -> particle_effect_handle {
  return particle_effect_handle{std::make_shared<particle_effect>(create_info)};
}

auto asset_residency::update_particle_effect(particle_effect_handle& effect, const particle_effect::create_info& create_info) -> void {
  if (!effect.is_valid()) {
    return;
  }

  effect->_emitters = create_info.emitters;
  effect->_name = create_info.name;
}

auto asset_residency::save_particle_effect(particle_effect_handle& effect, const std::filesystem::path& path) -> math::uuid {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!effect.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid particle_effect to '{}'", resolved_path.generic_string());
    return math::uuid::nil();
  }

  auto node = YAML::Node{};

  node["name"] = effect->name();

  auto emitters_node = YAML::Node{YAML::NodeType::Sequence};

  // Same idea as save_material's path_of lambda: re-relativize the import()-ed path for
  // load_texture(path, ...); nil-uuid (no texture assigned) omits the key entirely.
  const auto texture_path_of = [this](const texture_handle& texture) -> std::optional<std::string> {
    if (!texture.is_valid()) {
      return std::nullopt;
    }

    const auto absolute = _cooker.path_of(texture->id());

    if (absolute.empty()) {
      return std::nullopt;
    }

    return _cooker.relative(absolute).generic_string();
  };

  // Same idea as texture_path_of, generalized -- mesh_handle/material_handle share the same
  // is_valid()/->id() shape.
  const auto asset_path_of = [this](const auto& handle) -> std::optional<std::string> {
    if (!handle.is_valid()) {
      return std::nullopt;
    }

    const auto absolute = _cooker.path_of(handle->id());

    if (absolute.empty()) {
      return std::nullopt;
    }

    return _cooker.relative(absolute).generic_string();
  };

  const auto save_curve = [](const curve& value) -> YAML::Node {
    auto node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& key : value.keys) {
      auto key_node = YAML::Node{};
      key_node["time"] = key.time;
      key_node["value"] = key.value;
      node.push_back(key_node);
    }

    return node;
  };

  const auto save_gradient = [](const gradient& value) -> YAML::Node {
    auto node = YAML::Node{};

    auto color_keys_node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& key : value.color_keys) {
      auto key_node = YAML::Node{};
      key_node["time"] = key.time;
      key_node["color"] = key.color;
      color_keys_node.push_back(key_node);
    }

    node["color_keys"] = color_keys_node;

    auto alpha_keys_node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& key : value.alpha_keys) {
      auto key_node = YAML::Node{};
      key_node["time"] = key.time;
      key_node["alpha"] = key.alpha;
      alpha_keys_node.push_back(key_node);
    }

    node["alpha_keys"] = alpha_keys_node;

    return node;
  };

  for (const auto& emitter : effect->emitters()) {
    auto emitter_node = YAML::Node{};

    emitter_node["name"] = emitter.name;
    emitter_node["blend_mode"] = (emitter.blend_mode == emitter_blend_mode::alpha_blend) ? "alpha_blend" : "additive";
    emitter_node["simulation_mode"] = (emitter.simulation_mode == particle_simulation_mode::gpu) ? "gpu" : "cpu";
    emitter_node["emission_rate"] = emitter.emission_rate;
    emitter_node["burst_count"] = emitter.burst_count;
    emitter_node["shape"] = (emitter.shape == emitter_shape::sphere) ? "sphere" : (emitter.shape == emitter_shape::box) ? "box" : (emitter.shape == emitter_shape::cone) ? "cone" : "point";
    emitter_node["shape_extents"] = emitter.shape_extents;

    auto cone_node = YAML::Node{};
    cone_node["angle_degrees"] = emitter.cone.angle.to_degrees().value();
    cone_node["radius"] = emitter.cone.radius;
    cone_node["emit_from_volume"] = emitter.cone.emit_from_volume;
    emitter_node["cone"] = cone_node;

    emitter_node["velocity_min"] = emitter.velocity_min;
    emitter_node["velocity_max"] = emitter.velocity_max;
    emitter_node["lifetime_min"] = emitter.lifetime_min;
    emitter_node["lifetime_max"] = emitter.lifetime_max;
    emitter_node["start_color"] = emitter.start_color;
    emitter_node["end_color"] = emitter.end_color;
    emitter_node["color_over_lifetime"] = save_gradient(emitter.color_over_lifetime);
    emitter_node["size_min"] = emitter.size_min;
    emitter_node["size_max"] = emitter.size_max;
    emitter_node["size_over_lifetime"] = save_curve(emitter.size_over_lifetime);
    emitter_node["rotation_min"] = emitter.rotation_min;
    emitter_node["rotation_max"] = emitter.rotation_max;
    emitter_node["rotation_over_lifetime"] = save_curve(emitter.rotation_over_lifetime);

    auto velocity_curve_node = YAML::Node{};
    velocity_curve_node["x"] = save_curve(emitter.velocity_over_lifetime.x);
    velocity_curve_node["y"] = save_curve(emitter.velocity_over_lifetime.y);
    velocity_curve_node["z"] = save_curve(emitter.velocity_over_lifetime.z);
    emitter_node["velocity_over_lifetime"] = velocity_curve_node;

    emitter_node["force_over_lifetime_min"] = emitter.force_over_lifetime_min;
    emitter_node["force_over_lifetime_max"] = emitter.force_over_lifetime_max;

    emitter_node["gravity"] = emitter.gravity;
    emitter_node["drag"] = emitter.drag;

    if (const auto slot = texture_path_of(emitter.texture)) {
      emitter_node["texture"] = *slot;
    }

    emitter_node["render_mode"] = (emitter.render_mode == particle_render_mode::mesh) ? "mesh" : "billboard";

    if (const auto slot = asset_path_of(emitter.render_mesh)) {
      emitter_node["render_mesh"] = *slot;
    }

    if (const auto slot = asset_path_of(emitter.render_material)) {
      emitter_node["render_material"] = *slot;
    }

    auto collision_node = YAML::Node{};
    collision_node["mode"] = (emitter.collision.mode == particle_collision_mode::planes) ? "planes" : (emitter.collision.mode == particle_collision_mode::world) ? "world" : "none";
    collision_node["bounce"] = emitter.collision.bounce;
    collision_node["lifetime_loss"] = emitter.collision.lifetime_loss;
    collision_node["dampen"] = emitter.collision.dampen;
    collision_node["radius_scale"] = emitter.collision.radius_scale;
    collision_node["max_collisions_per_particle"] = emitter.collision.max_collisions_per_particle;

    auto planes_node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& plane : emitter.collision.planes) {
      auto plane_node = YAML::Node{};
      plane_node["normal"] = plane.normal;
      plane_node["distance"] = plane.distance;
      planes_node.push_back(plane_node);
    }

    collision_node["planes"] = planes_node;
    emitter_node["collision"] = collision_node;

    auto sub_emitters_node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& binding : emitter.sub_emitters) {
      auto binding_node = YAML::Node{};
      binding_node["event"] = (binding.event == sub_emitter_event::death) ? "death" : (binding.event == sub_emitter_event::collision) ? "collision" : "birth";

      if (const auto slot = asset_path_of(binding.effect)) {
        binding_node["effect"] = *slot;
      }

      binding_node["probability"] = binding.probability;
      binding_node["inherit_velocity"] = binding.inherit_velocity;
      sub_emitters_node.push_back(binding_node);
    }

    emitter_node["sub_emitters"] = sub_emitters_node;

    auto trail_node = YAML::Node{};
    trail_node["enabled"] = emitter.trail.enabled;
    trail_node["min_vertex_distance"] = emitter.trail.min_vertex_distance;
    trail_node["lifetime"] = emitter.trail.lifetime;
    trail_node["width"] = emitter.trail.width;
    trail_node["color_over_trail"] = save_gradient(emitter.trail.color_over_trail);
    trail_node["die_with_particle"] = emitter.trail.die_with_particle;
    emitter_node["trail"] = trail_node;

    emitters_node.push_back(emitter_node);
  }

  node["emitters"] = emitters_node;

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = _cooker.import(resolved_path);

  effect->_id = id;

  utility::logger<"assets">::info("Saved particle_effect '{}'", resolved_path.generic_string());

  return id;
}

// "type"+"value" tag pair -- animation_parameter_value's alternative *is* its type, so this is
// purely a persistence detail (the runtime API never switches on a type enum, see
// animation_graph.hpp's doc comment).
auto save_animation_parameter_value(const animation_parameter_value& value) -> YAML::Node {
  auto node = YAML::Node{};

  std::visit([&node](const auto& alternative) {
    using alternative_type = std::decay_t<decltype(alternative)>;

    if constexpr (std::is_same_v<alternative_type, std::float_t>) {
      node["type"] = "float";
      node["value"] = alternative;
    } else if constexpr (std::is_same_v<alternative_type, bool>) {
      node["type"] = "bool";
      node["value"] = alternative;
    } else if constexpr (std::is_same_v<alternative_type, std::int32_t>) {
      node["type"] = "int";
      node["value"] = alternative;
    } else {
      static_assert(std::is_same_v<alternative_type, animation_trigger>);
      node["type"] = "trigger"; // no value -- a trigger only ever carries a live "fired" flag, which isn't authored
    }
  }, value);

  return node;
}

auto load_animation_parameter_value(const YAML::Node& node) -> animation_parameter_value {
  const auto type = node["type"] ? node["type"].as<std::string>() : std::string{"float"};

  if (type == "bool") {
    return animation_parameter_value{node["value"] ? node["value"].as<bool>() : false};
  }

  if (type == "int") {
    return animation_parameter_value{node["value"] ? node["value"].as<std::int32_t>() : std::int32_t{0}};
  }

  if (type == "trigger") {
    return animation_parameter_value{animation_trigger{}};
  }

  return animation_parameter_value{node["value"] ? node["value"].as<std::float_t>() : 0.0f};
}

auto save_animation_condition_comparator(animation_condition_comparator comparator) -> std::string {
  switch (comparator) {
    case animation_condition_comparator::not_equals: return "not_equals";
    case animation_condition_comparator::greater: return "greater";
    case animation_condition_comparator::greater_or_equal: return "greater_or_equal";
    case animation_condition_comparator::less: return "less";
    case animation_condition_comparator::less_or_equal: return "less_or_equal";
    default: return "equals";
  }
}

auto load_animation_condition_comparator(const std::string& value) -> animation_condition_comparator {
  if (value == "not_equals") return animation_condition_comparator::not_equals;
  if (value == "greater") return animation_condition_comparator::greater;
  if (value == "greater_or_equal") return animation_condition_comparator::greater_or_equal;
  if (value == "less") return animation_condition_comparator::less;
  if (value == "less_or_equal") return animation_condition_comparator::less_or_equal;
  return animation_condition_comparator::equals;
}

auto asset_residency::load_animation_graph(const math::uuid& id) -> animation_graph_handle {
  _cooker.ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _animation_graph_files.find(id); entry != _animation_graph_files.end()) {
      return animation_graph_handle{entry->second};
    }
  }

  const auto source_path = _cooker.path_of(id);

  if (source_path.empty() || source_path.extension() != ".animation_graph") {
    utility::logger<"assets">::warn("Unknown animation_graph uuid {}", id);
    return animation_graph_handle{};
  }

  const auto& path = source_path;

  auto root = YAML::Node{};
  try {
    root = YAML::LoadFile(path.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse animation_graph '{}' ({})", path.generic_string(), exception.what());
    return animation_graph_handle{};
  }

  auto info = animation_graph::create_info{};

  if (root["name"]) info.name = root["name"].as<std::string>();
  if (root["entry_state_id"]) info.entry_state_id = root["entry_state_id"].as<std::uint32_t>();

  if (const auto parameters = root["parameters"]) {
    info.parameters.reserve(parameters.size());

    for (const auto parameter_node : parameters) {
      auto parameter = animation_parameter{};

      if (parameter_node["name"]) parameter.name = parameter_node["name"].as<std::string>();
      parameter.default_value = load_animation_parameter_value(parameter_node);

      info.parameters.push_back(parameter);
    }
  }

  if (const auto states = root["states"]) {
    info.states.reserve(states.size());

    for (const auto state_node : states) {
      auto state = animation_state{};

      if (state_node["id"]) state.id = state_node["id"].as<std::uint32_t>();
      if (state_node["name"]) state.name = state_node["name"].as<std::string>();
      if (state_node["clip_name"]) state.clip_name = state_node["clip_name"].as<std::string>();
      if (state_node["speed"]) state.speed = state_node["speed"].as<std::float_t>();
      if (state_node["loop"]) state.loop = state_node["loop"].as<bool>();

      if (const auto position_node = state_node["editor_position"]) {
        if (position_node["x"]) state.editor_position.x() = position_node["x"].as<std::float_t>();
        if (position_node["y"]) state.editor_position.y() = position_node["y"].as<std::float_t>();
      }

      info.states.push_back(state);
    }
  }

  if (const auto transitions = root["transitions"]) {
    info.transitions.reserve(transitions.size());

    for (const auto transition_node : transitions) {
      auto transition = animation_transition{};

      if (transition_node["from_state"]) transition.from_state = transition_node["from_state"].as<std::uint32_t>();
      if (transition_node["to_state"]) transition.to_state = transition_node["to_state"].as<std::uint32_t>();
      if (transition_node["duration"]) transition.duration = transition_node["duration"].as<std::float_t>();
      if (transition_node["has_exit_time"]) transition.has_exit_time = transition_node["has_exit_time"].as<bool>();
      if (transition_node["exit_time"]) transition.exit_time = transition_node["exit_time"].as<std::float_t>();

      if (const auto conditions_node = transition_node["conditions"]) {
        for (const auto condition_node : conditions_node) {
          auto condition = animation_condition{};

          if (condition_node["parameter_name"]) condition.parameter_name = condition_node["parameter_name"].as<std::string>();
          if (condition_node["comparator"]) condition.comparator = load_animation_condition_comparator(condition_node["comparator"].as<std::string>());
          condition.expected = load_animation_parameter_value(condition_node);

          transition.conditions.push_back(condition);
        }
      }

      info.transitions.push_back(transition);
    }
  }

  auto record = std::make_shared<animation_graph>(info);
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _animation_graph_files.emplace(id, record);
  }

  utility::logger<"assets">::info("Loaded animation_graph '{}'", path.generic_string());

  return animation_graph_handle{record};
}

auto asset_residency::load_animation_graph(const std::filesystem::path& path) -> animation_graph_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_animation_graph(_cooker.import(assets_directory / path));
}

auto asset_residency::create_animation_graph(const animation_graph::create_info& create_info) -> animation_graph_handle {
  return animation_graph_handle{std::make_shared<animation_graph>(create_info)};
}

auto asset_residency::update_animation_graph(animation_graph_handle& graph, const animation_graph::create_info& create_info) -> void {
  if (!graph.is_valid()) {
    return;
  }

  graph->_name = create_info.name;
  graph->_parameters = create_info.parameters;
  graph->_states = create_info.states;
  graph->_transitions = create_info.transitions;
  graph->_entry_state_id = create_info.entry_state_id;
}

auto asset_residency::save_animation_graph(animation_graph_handle& graph, const std::filesystem::path& path) -> math::uuid {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!graph.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid animation_graph to '{}'", resolved_path.generic_string());
    return math::uuid::nil();
  }

  auto node = YAML::Node{};

  node["name"] = graph->name();
  node["entry_state_id"] = graph->entry_state_id();

  auto parameters_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& parameter : graph->parameters()) {
    auto parameter_node = save_animation_parameter_value(parameter.default_value);
    parameter_node["name"] = parameter.name;
    parameters_node.push_back(parameter_node);
  }

  node["parameters"] = parameters_node;

  auto states_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& state : graph->states()) {
    auto state_node = YAML::Node{};

    state_node["id"] = state.id;
    state_node["name"] = state.name;
    state_node["clip_name"] = state.clip_name;
    state_node["speed"] = state.speed;
    state_node["loop"] = state.loop;

    auto position_node = YAML::Node{};
    position_node["x"] = state.editor_position.x();
    position_node["y"] = state.editor_position.y();
    state_node["editor_position"] = position_node;

    states_node.push_back(state_node);
  }

  node["states"] = states_node;

  auto transitions_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& transition : graph->transitions()) {
    auto transition_node = YAML::Node{};

    if (transition.from_state) {
      transition_node["from_state"] = *transition.from_state;
    }

    transition_node["to_state"] = transition.to_state;
    transition_node["duration"] = transition.duration;
    transition_node["has_exit_time"] = transition.has_exit_time;
    transition_node["exit_time"] = transition.exit_time;

    auto conditions_node = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& condition : transition.conditions) {
      auto condition_node = save_animation_parameter_value(condition.expected);
      condition_node["parameter_name"] = condition.parameter_name;
      condition_node["comparator"] = save_animation_condition_comparator(condition.comparator);
      conditions_node.push_back(condition_node);
    }

    transition_node["conditions"] = conditions_node;

    transitions_node.push_back(transition_node);
  }

  node["transitions"] = transitions_node;

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = _cooker.import(resolved_path);

  graph->_id = id;

  utility::logger<"assets">::info("Saved animation_graph '{}'", resolved_path.generic_string());

  return id;
}

auto asset_residency::load_environment_map(const math::uuid& id) -> environment_map_handle {
  auto timer = utility::scoped_timer{[&id](const units::seconds& elapsed) {
    utility::logger<"assets">::info("Loaded environment map {} in {}", id, units::milliseconds{elapsed});
  }};

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _environment_maps.find(id); entry != _environment_maps.end()) {
      return environment_map_handle{entry->second};
    }
  }

  auto data = _cooker.resolve_environment(id);

  if (!data) {
    return environment_map_handle{};
  }

  auto record = std::make_shared<environment_map>();
  record->_id = id;

  // Bakes irradiance + prefiltered via compute and blocks until the GPU finishes, so the
  // environment is fully usable the moment this call returns (load-time bake, not lazy first-frame).
  _ibl.bake_environment(*record, data->pixels, data->width, data->height);

  {
    auto lock = std::lock_guard{_mutex};
    _environment_maps.emplace(id, record);
  }

  return environment_map_handle{record};
}

auto asset_residency::load_environment_map(const std::filesystem::path& path) -> environment_map_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_environment_map(_cooker.import(assets_directory / path));
}

auto asset_residency::process_uploads(std::uint64_t frame_index) -> void {
  auto pending_textures = std::vector<pending_texture_upload>{};
  auto pending_meshes = std::vector<pending_mesh_upload>{};
  auto pending_materials = std::vector<pending_material_upload>{};

  {
    auto lock = std::lock_guard{_mutex};
    pending_textures.swap(_pending_textures);
    pending_meshes.swap(_pending_meshes);
    pending_materials.swap(_pending_materials);
  }

  if (pending_textures.empty() && pending_meshes.empty() && pending_materials.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& upload_context = graphics_module.upload_context();
  auto& bindless_table = graphics_module.bindless_table();

  for (auto& request : pending_textures) {
    const auto mip_levels = graphics::image::mip_levels_for(math::vector3u{request.width, request.height, 1u});

    const auto handle = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{request.width, request.height, 1u},
      .format = request.format,
      .usage = graphics::image_usage::transfer_destination | graphics::image_usage::transfer_source | graphics::image_usage::sampled,
      .mip_levels = mip_levels,
      .name = "Texture"
    });

    const auto bytes = std::span<const std::byte>{request.pixels.data(), request.pixels.size()};

    upload_context.stage_image(handle, bytes, graphics::image_layout::shader_read_only_optimal);

    bindless_table.write_sampled_image(request.index, registry.get<graphics::image>(handle).view());

    _images.emplace(request.index, handle);
    _resident_frame.emplace(request.index, frame_index);
  }

  for (auto& request : pending_meshes) {
    const auto vertex_bytes = static_cast<graphics::buffer::size_type>(request.vertices.size() * sizeof(vertex));
    const auto index_bytes = static_cast<graphics::buffer::size_type>(request.indices.size() * sizeof(std::uint32_t));

    const auto vertex_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = vertex_bytes,
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::transfer_destination,
      .memory = graphics::memory_usage::device_local,
      .name = "Mesh Vertices"
    });

    const auto index_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = index_bytes,
      .usage = graphics::buffer_usage::index | graphics::buffer_usage::transfer_destination,
      .memory = graphics::memory_usage::device_local,
      .name = "Mesh Indices"
    });

    upload_context.stage_buffer(vertex_buffer, std::as_bytes(std::span{request.vertices}));
    upload_context.stage_buffer(index_buffer, std::as_bytes(std::span{request.indices}));

    const auto vertex_address = registry.get<graphics::buffer>(vertex_buffer).address();

    if (!request.skin_vertices.empty()) {
      const auto skin_vertex_bytes = static_cast<graphics::buffer::size_type>(request.skin_vertices.size() * sizeof(skin_vertex));

      const auto skin_vertex_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
        .size = skin_vertex_bytes,
        .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::transfer_destination,
        .memory = graphics::memory_usage::device_local,
        .name = "Mesh Skin Vertices"
      });

      upload_context.stage_buffer(skin_vertex_buffer, std::as_bytes(std::span{request.skin_vertices}));

      const auto skin_vertex_address = registry.get<graphics::buffer>(skin_vertex_buffer).address();

      request.record->_finalize(vertex_buffer, index_buffer, vertex_address, frame_index, skin_vertex_buffer, skin_vertex_address);
    } else {
      request.record->_finalize(vertex_buffer, index_buffer, vertex_address, frame_index);
    }
  }

  // Create the material buffer once, sized for the whole capacity.
  if (!_material_buffer.is_valid()) {
    _material_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = material_capacity * memory::stride_v<material_data>,
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Material Data"
    });

    _material_address = registry.get<graphics::buffer>(_material_buffer).address();
  }

  auto& buffer = registry.get<graphics::buffer>(_material_buffer);

  for (auto& request : pending_materials) {
    const auto& material = *request.record;

    const auto resolve = [this](const texture_handle& texture, const texture_handle& fallback) {
      return texture.is_valid() ? texture->index() : fallback->index();
    };

    const auto& base_color_factor = material.base_color_factor();
    const auto& emissive_factor = material.emissive_factor();

    auto data = material_data{};
    data.base_color_factor = math::vector4{base_color_factor.r(), base_color_factor.g(), base_color_factor.b(), base_color_factor.a()};
    data.emissive_factor = math::vector4{emissive_factor.x(), emissive_factor.y(), emissive_factor.z(), 0.0f};
    data.albedo_index = resolve(material.albedo(), _white);
    data.normal_index = resolve(material.normal(), _normal);
    data.metallic_roughness_index = resolve(material.metallic_roughness(), _white);
    data.occlusion_index = resolve(material.occlusion(), _white);
    data.emissive_index = resolve(material.emissive(), _white);
    data.metallic_factor = material.metallic_factor();
    data.roughness_factor = material.roughness_factor();
    data.alpha_cutoff = material.alpha_cutoff();
    data.flags = ((material.alpha() == alpha_mode::mask) ? material_flag_masked : 0u) | (material.receives_shadow() ? material_flag_receives_shadow : 0u);
    data.normal_scale = material.normal_scale();
    data.occlusion_strength = material.occlusion_strength();
    data.emissive_strength = material.emissive_strength();
    data.ior = material.ior();

    buffer.write(&data, sizeof(material_data), material.index() * memory::stride_v<material_data>);
  }
}

auto asset_residency::is_resident(const texture_handle& texture) const -> bool {
  if (!texture.is_valid()) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& frame_context = graphics_module.frame_context();

  const auto completed_value = frame_context.timeline_value();

  auto lock = std::lock_guard{_mutex};

  const auto entry = _resident_frame.find(texture->index());

  return entry != _resident_frame.end() && completed_value >= entry->second;
}

auto asset_residency::is_resident(const mesh_handle& mesh) const -> bool {
  if (!mesh.is_valid() || !mesh->is_uploaded()) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto value = graphics_module.frame_context().timeline_value();

  return value >= mesh->resident_frame();
}

auto asset_residency::is_resident(const material_handle& material) const -> bool {
  if (!material.is_valid()) {
    return false;
  }

  const auto is_ready = [this](const texture_handle& texture) -> bool {
    return !texture.is_valid() || is_resident(texture);
  };

  return is_ready(material->albedo()) && is_ready(material->normal()) && is_ready(material->metallic_roughness()) && is_ready(material->occlusion()) && is_ready(material->emissive());
}

auto asset_residency::is_resident(const environment_map_handle& environment) const -> bool {
  // bake_environment blocks until the GPU finishes, so a valid handle is always fully resident —
  // no timeline wait needed here, unlike textures/meshes/materials' deferred per-frame upload.
  return environment.is_valid();
}

auto asset_residency::image_view_of(const texture_handle& texture) const -> VkImageView {
  if (!texture.is_valid()) {
    return VK_NULL_HANDLE;
  }

  auto lock = std::lock_guard{_mutex};

  const auto entry = _images.find(texture->index());

  if (entry == _images.end()) {
    return VK_NULL_HANDLE;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  return registry.get<graphics::image>(entry->second).view();
}

auto asset_residency::_create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});

  auto pixels = std::vector<std::byte>{
    std::byte{color[0]}, std::byte{color[1]}, std::byte{color[2]}, std::byte{color[3]}
  };

  {
    auto lock = std::lock_guard{_mutex};
    _pending_textures.push_back(pending_texture_upload{index, std::move(pixels), 1u, 1u, graphics::format::r8g8b8a8_unorm});
  }

  return texture_handle{record};
}

auto asset_residency::_register_material(std::shared_ptr<material> record) -> material_handle {
  auto lock = std::lock_guard{_mutex};

  utility::assert_that(_material_count < material_capacity, "Exceeded material capacity");

  record->_index = _material_count++;

  _materials.push_back(record);
  _pending_materials.push_back(pending_material_upload{record});

  return material_handle{record};
}

auto asset_residency::_extract_gltf_material(const material_description& description, const std::filesystem::path& relative_source) -> math::uuid {
  // relative_source is fully resolved; save_material/load_material(path) expect an assets-relative
  // input, so re-relativize it here (same as the editor's extract_material_to_asset).
  const auto source_relative = _cooker.relative(relative_source);

  const auto directory = source_relative.parent_path() / "materials"; // mirrors textures already landing in models/<name>/textures/
  const auto relative_path = directory / (sanitize_file_name(description.name.empty() ? "material" : description.name) + ".material");

  // Already extracted (possibly hand-edited since a previous cook) — reuse it as-is, never overwrite.
  if (std::filesystem::exists(_cooker.absolute(relative_path))) {
    if (auto existing = load_material(relative_path); existing.is_valid()) {
      return existing->id();
    }
  }

  auto info = material::create_info{};
  info.name = description.name.empty() ? "material" : description.name;
  info.base_color_factor = description.base_color_factor;
  info.emissive_factor = description.emissive_factor;
  info.metallic_factor = description.metallic_factor;
  info.roughness_factor = description.roughness_factor;
  info.alpha = description.alpha;
  info.alpha_cutoff = description.alpha_cutoff;
  info.is_double_sided = description.is_double_sided;

  const auto load_slot = [&](const math::uuid& uuid, graphics::format format) -> texture_handle {
    return (uuid == math::uuid::nil()) ? texture_handle{} : load_texture(uuid, format);
  };

  info.albedo = load_slot(description.albedo, graphics::format::r8g8b8a8_srgb);
  info.normal = load_slot(description.normal, graphics::format::r8g8b8a8_unorm);
  info.metallic_roughness = load_slot(description.metallic_roughness, graphics::format::r8g8b8a8_unorm);
  info.occlusion = load_slot(description.occlusion, graphics::format::r8g8b8a8_unorm);
  info.emissive = load_slot(description.emissive, graphics::format::r8g8b8a8_srgb);

  auto handle = create_material(info);

  return save_material(handle, relative_path);
}

} // namespace sbx::assets
