// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_residency.hpp>

#include <algorithm>
#include <fstream>
#include <type_traits>
#include <utility>
#include <variant>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/primitive_meshes.hpp>

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
  math::vector2 uv_tiling;
  math::vector2 uv_offset;

  // Shader-graph-exposed parameters -- unused (always zero) until a material actually references a
  // shader_graph (material opt-in, still future work; see the shader graph plan's "Material
  // opt-in"/"GPU parameter storage" sections). Added now so a hand-authored graph using an exposed
  // constant_* or texture_sample node compiles today, not only once that wiring lands -- codegen
  // (shader_graph_codegen.cpp) already emits `material.generic_params[N]`/`generic_textures[N]`
  // reads for those node types. One uniform float4 slot per param regardless of its actual type
  // (scalar/vector3/color use .x/.xyz/.rgba of their slot) avoids sub-float packing/alignment
  // footguns for a handful of extra bytes per material.
  math::vector4 generic_params[shader_graph_max_params];
  std::uint32_t generic_textures[shader_graph_max_textures];
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

// "type"+"value" tag pair -- animation_parameter_value's alternative *is* its type, so this is
// purely a persistence detail (the runtime API never switches on a type enum, see
// animation_graph.hpp's doc comment). Mirrored by load_animation_parameter_value in
// asset_cooker_animation_graph.cpp -- parsing moved there with the rest of that asset kind's cooking;
// saving stays here since it's a synchronous, editor-only write path.
static auto save_animation_parameter_value(const animation_parameter_value& value) -> YAML::Node {
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

static auto save_animation_condition_comparator(animation_condition_comparator comparator) -> std::string {
  switch (comparator) {
    case animation_condition_comparator::not_equals: return "not_equals";
    case animation_condition_comparator::greater: return "greater";
    case animation_condition_comparator::greater_or_equal: return "greater_or_equal";
    case animation_condition_comparator::less: return "less";
    case animation_condition_comparator::less_or_equal: return "less_or_equal";
    default: return "equals";
  }
}

asset_residency::asset_residency(asset_manifest& manifest, ibl_baker& baker)
: _manifest{manifest},
  _ibl{baker} {
  _white = _create_default_texture({255u, 255u, 255u, 255u});
  _normal = _create_default_texture({128u, 128u, 255u, 255u}); // (0,0,1) tangent-space normal
  _black = _create_default_texture({0u, 0u, 0u, 255u});
  _magenta = _create_default_texture({255u, 0u, 255u, 255u});   // load-error marker
}

asset_residency::~asset_residency() {
  // Stop the background thread *before* any of the caches/pending-upload queues above it are torn
  // down -- see the member declaration's own doc comment. The member's own destructor would call
  // abort() anyway; this makes the shutdown-ordering intent explicit at this level too.
  _loader.abort();
}

auto asset_residency::_texture_cache_key(const math::uuid& id, graphics::format format) const -> std::string {
  const auto is_srgb = (format == graphics::format::r8g8b8a8_srgb);

  return fmt::format("{}:{}", id.value(), (is_srgb ? "#srgb" : "#linear"));
}

auto asset_residency::load_texture(const math::uuid& id, graphics::format format) -> texture_handle {
  const auto key = _texture_cache_key(id, format);

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _textures.find(key); entry != _textures.end()) {
      return texture_handle{entry->second};
    }
  }

  const auto source = _manifest.path_of(id);

  if (source.empty()) {
    utility::logger<"assets">::warn("Unknown texture uuid {}", id);
    return texture_handle{};
  }

  const auto cooked = _manifest.cooked_path(id, ".sbxtex");
  const auto needs_cook = _manifest.is_cooked_stale(id, source, cooked, texture_cook_version);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _textures.emplace(key, record);
  }

  _loader.submit(asset_loader::texture_request{id, format, source, cooked, needs_cook});

  return texture_handle{record};
}

auto asset_residency::load_texture(const std::filesystem::path& path, graphics::format format) -> texture_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_texture(_manifest.import(assets_directory / path), format);
}

auto asset_residency::load_font(const math::uuid& id) -> font_handle {
  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _fonts.find(id); entry != _fonts.end()) {
      return font_handle{entry->second};
    }
  }

  const auto source = _manifest.path_of(id);

  if (source.empty()) {
    utility::logger<"assets">::warn("Unknown font uuid {}", id);
    return font_handle{};
  }

  const auto cooked = _manifest.cooked_path(id, ".sbxfnt");
  const auto needs_cook = _manifest.is_cooked_stale(id, source, cooked, font_cook_version);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<font>();
  record->_atlas = texture_handle{std::make_shared<texture>(texture{index})};
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _fonts.emplace(id, record);
  }

  _loader.submit(asset_loader::font_request{id, source, cooked, needs_cook});

  return font_handle{record};
}

auto asset_residency::load_font(const std::filesystem::path& path) -> font_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_font(_manifest.import(assets_directory / path));
}

auto asset_residency::load_mesh(const math::uuid& id, const mesh_import_options& options, bool force_recook) -> mesh_handle {
  if (!force_recook) {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _meshes.find(id); entry != _meshes.end()) {
      return mesh_handle{entry->second};
    }
  }

  if (const auto kind = primitive_mesh_kind_of(id); kind.has_value()) {
    ensure_primitive_mesh_cooked(*kind);

    const auto cooked = asset_cooker::cooked_path(id, ".sbxmsh");

    auto record = std::make_shared<mesh>();
    record->_id = id;

    {
      auto lock = std::lock_guard{_mutex};
      _meshes.emplace(id, record);
    }

    _loader.submit(asset_loader::mesh_request{id, options, std::filesystem::path{}, cooked, false});

    return mesh_handle{record};
  }

  const auto source = _manifest.path_of(id);

  if (source.empty()) {
    utility::logger<"assets">::warn("Unknown mesh uuid {}", id);
    return mesh_handle{};
  }

  const auto cooked = _manifest.cooked_path(id, ".sbxmsh");
  const auto needs_cook = force_recook || _manifest.is_cooked_stale(id, source, cooked, mesh_cooker_version);

  // Reuses the already-resident record on a force_recook (emplace() below would otherwise be a
  // no-op against an existing key, silently stranding this freshly-made-but-never-submitted record
  // while _finalize_mesh keeps repopulating the *old* one by id) -- otherwise, same as always, a
  // fresh placeholder record for a mesh not yet seen this session.
  auto record = std::shared_ptr<mesh>{};

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _meshes.find(id); entry != _meshes.end()) {
      record = entry->second;
    } else {
      record = std::make_shared<mesh>();
      record->_id = id;
      _meshes.emplace(id, record);
    }
  }

  _loader.submit(asset_loader::mesh_request{id, options, source, cooked, needs_cook});

  return mesh_handle{record};
}

auto asset_residency::load_mesh(const std::filesystem::path& path, const mesh_import_options& options, bool force_recook) -> mesh_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_mesh(_manifest.import(assets_directory / path), options, force_recook);
}

auto asset_residency::create_mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, std::vector<mesh::submesh> submeshes, const math::volume& bounds) -> mesh_handle {
  const auto vertex_count = static_cast<std::uint32_t>(vertices.size());

  auto record = std::make_shared<mesh>(std::move(submeshes), bounds, vertex_count);
  record->_id = math::uuid::create();

  {
    auto lock = std::lock_guard{_mutex};
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(vertices), std::move(indices), {}});
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

  auto record = std::make_shared<skeleton>();
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _skeletons.emplace(id, record);
  }

  _loader.submit(asset_loader::skeleton_request{id});

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

  auto record = std::make_shared<animation_clip>();
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _animation_clips.emplace(id, record);
  }

  _loader.submit(asset_loader::animation_clip_request{id});

  return animation_clip_handle{record};
}

auto asset_residency::load_material(const math::uuid& id) -> material_handle {
  _manifest.ensure_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _material_files.find(id); entry != _material_files.end()) {
      return material_handle{entry->second};
    }
  }

  // A hand-authored `.material` file has a real, `.material`-suffixed path; a material cooked as a
  // side effect of a mesh import (a derived uuid, never separately import()-ed) has none -- the
  // background resolve (asset_loader::_resolve(material_request)) picks the same branch this would
  // have picked synchronously, given the same source path.
  const auto source_path = _manifest.path_of(id);

  auto record = std::make_shared<material>(material::create_info{});
  record->_id = id;

  auto handle = _register_material(record); // reserves the UBO index, queues an initial (empty) upload

  {
    auto lock = std::lock_guard{_mutex};
    _material_files.emplace(id, record);
  }

  _loader.submit(asset_loader::material_request{id, (source_path.extension() == ".material") ? source_path : std::filesystem::path{}});

  return handle;
}

auto asset_residency::load_material(const std::filesystem::path& path) -> material_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_material(_manifest.import(assets_directory / path));
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
  material->_shading = create_info.shading;
  material->_alpha_cutoff = create_info.alpha_cutoff;
  material->_is_double_sided = create_info.is_double_sided;
  material->_casts_shadow = create_info.casts_shadow;
  material->_receives_shadow = create_info.receives_shadow;
  material->_normal_scale = create_info.normal_scale;
  material->_occlusion_strength = create_info.occlusion_strength;
  material->_emissive_strength = create_info.emissive_strength;
  material->_ior = create_info.ior;
  material->_uv_tiling = create_info.uv_tiling;
  material->_uv_offset = create_info.uv_offset;
  material->_albedo = create_info.albedo;
  material->_normal = create_info.normal;
  material->_metallic_roughness = create_info.metallic_roughness;
  material->_occlusion = create_info.occlusion;
  material->_emissive = create_info.emissive;
  material->_shader_graph = create_info.shader_graph;
  material->_generic_params = create_info.generic_params;
  material->_generic_textures = create_info.generic_textures;
  material->_name = create_info.name;

  // Covers both the async-finalize path (_finalize_material calling this) and a live editor edit
  // -- both are real content changes anything checking is_loaded()/generation() should see.
  material->_bump_generation();

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

    const auto absolute = _manifest.path_of(texture->id());

    if (absolute.empty()) {
      return std::nullopt; // default/procedural texture (nil uuid) — omit the slot
    }

    // absolute is stored fully resolved; the slot needs to hold the assets-relative form (that's
    // what load_material's own reader passes straight into load_texture(path, ...)).
    return _manifest.relative(absolute).generic_string();
  };

  auto node = YAML::Node{};

  node["name"] = material->name();
  node["base_color_factor"] = material->base_color_factor();
  node["emissive_factor"] = material->emissive_factor();
  node["metallic_factor"] = material->metallic_factor();
  node["roughness_factor"] = material->roughness_factor();
  node["alpha_mode"] = (material->alpha() == alpha_mode::blend) ? "blend" : (material->alpha() == alpha_mode::mask) ? "mask" : "opaque";
  node["shading_model"] = (material->shading() == shading_model::unlit) ? "unlit" : (material->shading() == shading_model::shader_graph) ? "shader_graph" : "pbr";
  node["alpha_cutoff"] = material->alpha_cutoff();
  node["is_double_sided"] = material->is_double_sided();
  node["casts_shadow"] = material->casts_shadow();
  node["receives_shadow"] = material->receives_shadow();
  node["normal_scale"] = material->normal_scale();
  node["occlusion_strength"] = material->occlusion_strength();
  node["emissive_strength"] = material->emissive_strength();
  node["ior"] = material->ior();
  node["uv_tiling"] = material->uv_tiling();
  node["uv_offset"] = material->uv_offset();

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

  if (material->shader_graph().is_valid()) {
    const auto absolute = _manifest.path_of(material->shader_graph()->id());

    if (!absolute.empty()) {
      node["shader_graph"] = _manifest.relative(absolute).generic_string();
    }
  }

  auto generic_params_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& value : material->generic_params()) {
    generic_params_node.push_back(value);
  }

  node["generic_params"] = generic_params_node;

  auto generic_textures_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& texture : material->generic_textures()) {
    generic_textures_node.push_back(path_of(texture).value_or(std::string{}));
  }

  node["generic_textures"] = generic_textures_node;

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = _manifest.import(resolved_path); // register + create the .meta so it's a first-class asset

  // import() is idempotent (returns the existing uuid from .meta on a re-save), so this always
  // stamps the right id — including a create_material()'d material's first save (nil id otherwise).
  material->_id = id;

  {
    // Without this, a later load_material(id) (e.g. dragging the same material tile again) finds
    // no cache entry and mints a second, independent record for the same uuid — a duplicate GPU
    // material-buffer slot that never reflects edits made through this handle.
    auto lock = std::lock_guard{_mutex};
    _material_files[id] = material.shared();
  }

  utility::logger<"assets">::info("Saved material '{}'", resolved_path.generic_string());

  return id;
}

auto asset_residency::load_particle_effect(const math::uuid& id) -> particle_effect_handle {
  _manifest.ensure_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _particle_effect_files.find(id); entry != _particle_effect_files.end()) {
      return particle_effect_handle{entry->second};
    }
  }

  const auto source_path = _manifest.path_of(id);

  if (source_path.empty() || source_path.extension() != ".particle_effect") {
    utility::logger<"assets">::warn("Unknown particle_effect uuid {}", id);
    return particle_effect_handle{};
  }

  auto record = std::make_shared<particle_effect>();
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _particle_effect_files.emplace(id, record);
  }

  _loader.submit(asset_loader::particle_effect_request{id, source_path});

  return particle_effect_handle{record};
}

auto asset_residency::load_particle_effect(const std::filesystem::path& path) -> particle_effect_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_particle_effect(_manifest.import(assets_directory / path));
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
  effect->_bump_generation();
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

    const auto absolute = _manifest.path_of(texture->id());

    if (absolute.empty()) {
      return std::nullopt;
    }

    return _manifest.relative(absolute).generic_string();
  };

  // Same idea as texture_path_of, generalized -- mesh_handle/material_handle share the same
  // is_valid()/->id() shape.
  const auto asset_path_of = [this](const auto& handle) -> std::optional<std::string> {
    if (!handle.is_valid()) {
      return std::nullopt;
    }

    const auto absolute = _manifest.path_of(handle->id());

    if (absolute.empty()) {
      return std::nullopt;
    }

    return _manifest.relative(absolute).generic_string();
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

  const auto id = _manifest.import(resolved_path);

  effect->_id = id;

  {
    // See save_material's identical fix -- without this a later load_particle_effect(id) mints a
    // second, independent in-memory record for the same uuid instead of finding this one.
    auto lock = std::lock_guard{_mutex};
    _particle_effect_files[id] = effect.shared();
  }

  utility::logger<"assets">::info("Saved particle_effect '{}'", resolved_path.generic_string());

  return id;
}

auto asset_residency::load_animation_graph(const math::uuid& id) -> animation_graph_handle {
  _manifest.ensure_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _animation_graph_files.find(id); entry != _animation_graph_files.end()) {
      return animation_graph_handle{entry->second};
    }
  }

  const auto source_path = _manifest.path_of(id);

  if (source_path.empty() || source_path.extension() != ".animation_graph") {
    utility::logger<"assets">::warn("Unknown animation_graph uuid {}", id);
    return animation_graph_handle{};
  }

  auto record = std::make_shared<animation_graph>();
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _animation_graph_files.emplace(id, record);
  }

  _loader.submit(asset_loader::animation_graph_request{id, source_path});

  return animation_graph_handle{record};
}

auto asset_residency::load_animation_graph(const std::filesystem::path& path) -> animation_graph_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_animation_graph(_manifest.import(assets_directory / path));
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
  graph->_bump_generation();
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
    transition_node["exit_time"] = transition.exit_time.value();

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

  const auto id = _manifest.import(resolved_path);

  graph->_id = id;

  {
    // See save_material's identical fix -- without this a later load_animation_graph(id) mints a
    // second, independent in-memory record for the same uuid instead of finding this one.
    auto lock = std::lock_guard{_mutex};
    _animation_graph_files[id] = graph.shared();
  }

  utility::logger<"assets">::info("Saved animation_graph '{}'", resolved_path.generic_string());

  return id;
}

auto asset_residency::load_shader_graph(const math::uuid& id) -> shader_graph_handle {
  _manifest.ensure_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _shader_graph_files.find(id); entry != _shader_graph_files.end()) {
      return shader_graph_handle{entry->second};
    }
  }

  const auto source_path = _manifest.path_of(id);

  if (source_path.empty() || source_path.extension() != ".shadergraph") {
    utility::logger<"assets">::warn("Unknown shader_graph uuid {}", id);
    return shader_graph_handle{};
  }

  auto record = std::make_shared<shader_graph>();
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _shader_graph_files.emplace(id, record);
  }

  _loader.submit(asset_loader::shader_graph_request{id, source_path});

  return shader_graph_handle{record};
}

auto asset_residency::load_shader_graph(const std::filesystem::path& path) -> shader_graph_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_shader_graph(_manifest.import(assets_directory / path));
}

auto asset_residency::create_shader_graph(const shader_graph::create_info& create_info) -> shader_graph_handle {
  return shader_graph_handle{std::make_shared<shader_graph>(create_info)};
}

auto asset_residency::update_shader_graph_data(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void {
  if (!graph.is_valid()) {
    return;
  }

  graph->_name = create_info.name;
  graph->_nodes = create_info.nodes;
  graph->_edges = create_info.edges;
}

auto asset_residency::update_shader_graph(shader_graph_handle& graph, const shader_graph::create_info& create_info) -> void {
  if (!graph.is_valid()) {
    return;
  }

  update_shader_graph_data(graph, create_info);
  graph->_bump_generation();

  // shader_cache/pipeline_cache have no invalidation of their own -- the generation-suffixed path
  // from shader_graph_generated_path is what makes a re-cook actually take effect (see its own doc
  // comment), so the bump above has to happen before this cook, not after.
  auto info = shader_graph::create_info{};
  info.name = graph->name();
  info.nodes = graph->nodes();
  info.edges = graph->edges();

  if (!asset_cooker::cook_shader_graph(graph->id(), graph->generation(), info)) {
    utility::logger<"assets">::warn("shader_graph {} failed to cook -- see the warning above for why", graph->id());
  }
}

auto asset_residency::update_shader_graph_node_position(shader_graph_handle& graph, std::uint32_t node_id, math::vector2 position) -> void {
  if (!graph.is_valid()) {
    return;
  }

  const auto entry = std::ranges::find(graph->_nodes, node_id, &shader_graph_node::id);

  if (entry == graph->_nodes.end()) {
    return;
  }

  entry->editor_position = position;
}

auto asset_residency::save_shader_graph(shader_graph_handle& graph, const std::filesystem::path& path) -> math::uuid {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!graph.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid shader_graph to '{}'", resolved_path.generic_string());
    return math::uuid::nil();
  }

  const auto path_of = [this](const texture_handle& texture) -> std::optional<std::string> {
    if (!texture.is_valid()) {
      return std::nullopt;
    }

    const auto absolute = _manifest.path_of(texture->id());

    if (absolute.empty()) {
      return std::nullopt;
    }

    return _manifest.relative(absolute).generic_string();
  };

  auto node = YAML::Node{};

  node["name"] = graph->name();

  auto nodes_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& graph_node : graph->nodes()) {
    auto node_yaml = YAML::Node{};

    node_yaml["id"] = graph_node.id;
    node_yaml["type"] = shader_node_type_to_string(graph_node.type);
    node_yaml["position"] = graph_node.editor_position;
    node_yaml["name"] = graph_node.name;
    node_yaml["exposed"] = graph_node.exposed;
    node_yaml["preview"] = graph_node.preview;

    if (const auto* value = std::get_if<std::float_t>(&graph_node.value)) {
      node_yaml["value"] = *value;
    } else if (const auto* value = std::get_if<math::vector2>(&graph_node.value)) {
      node_yaml["value"] = *value;
    } else if (const auto* value = std::get_if<math::vector3>(&graph_node.value)) {
      node_yaml["value"] = *value;
    } else if (const auto* value = std::get_if<math::vector4>(&graph_node.value)) {
      node_yaml["value"] = *value;
    } else if (const auto* value = std::get_if<math::color>(&graph_node.value)) {
      node_yaml["value"] = *value;
    } else if (const auto* value = std::get_if<texture_handle>(&graph_node.value)) {
      if (const auto slot = path_of(*value)) {
        node_yaml["texture"] = *slot;
      }
    } else if (const auto* value = std::get_if<std::string>(&graph_node.value)) {
      node_yaml["pattern"] = *value; // Swizzle only -- the one node type whose value is a plain string
    }

    nodes_node.push_back(node_yaml);
  }

  node["nodes"] = nodes_node;

  auto edges_node = YAML::Node{YAML::NodeType::Sequence};

  for (const auto& edge : graph->edges()) {
    auto edge_yaml = YAML::Node{};

    edge_yaml["from_node"] = edge.from_node;
    edge_yaml["from_pin"] = edge.from_pin;
    edge_yaml["to_node"] = edge.to_node;
    edge_yaml["to_pin"] = edge.to_pin;

    edges_node.push_back(edge_yaml);
  }

  node["edges"] = edges_node;

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = _manifest.import(resolved_path);

  graph->_id = id;

  {
    auto lock = std::lock_guard{_mutex};
    _shader_graph_files[id] = graph.shared();
  }

  auto info = shader_graph::create_info{};
  info.name = graph->name();
  info.nodes = graph->nodes();
  info.edges = graph->edges();

  // Bumps the generation and re-cooks -- live editing no longer does either (see
  // update_shader_graph's own doc comment), so this is the one point a saved edit's shading result
  // actually starts taking effect.
  update_shader_graph(graph, info);

  utility::logger<"assets">::info("Saved shader_graph '{}'", resolved_path.generic_string());

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

  const auto source = _manifest.path_of(id);

  if (source.empty()) {
    utility::logger<"assets">::warn("Unknown environment map uuid {}", id);
    return environment_map_handle{};
  }

  const auto cooked = _manifest.cooked_path(id, ".sbxenv");
  const auto needs_cook = _manifest.is_cooked_stale(id, source, cooked, environment_cook_version);

  auto did_cook = false;
  auto data = asset_cooker::resolve_environment(source, cooked, needs_cook, did_cook);

  if (did_cook) {
    _manifest.record_cook(id, environment_cook_version, source);
  }

  if (!data) {
    return environment_map_handle{};
  }

  auto record = std::make_shared<environment_map>();
  record->_id = id;

  // Bakes irradiance + prefiltered via compute and blocks until the GPU finishes, so the
  // environment is fully usable the moment this call returns (load-time bake, not lazy first-frame).
  // Deliberately synchronous/main-thread only -- see this method's doc comment in the header.
  _ibl.bake_environment(*record, data->pixels, data->width, data->height);

  // Never a placeholder -- always fully baked by this point -- so this just makes it consistently
  // report is_loaded() == true immediately, same as everything else.
  record->_bump_generation();

  {
    auto lock = std::lock_guard{_mutex};
    _environment_maps.emplace(id, record);
  }

  return environment_map_handle{record};
}

auto asset_residency::load_environment_map(const std::filesystem::path& path) -> environment_map_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_environment_map(_manifest.import(assets_directory / path));
}

auto asset_residency::_drain_loader_results() -> void {
  auto remaining = max_uploads_per_frame;

  auto textures = _loader.take_resolved_textures(remaining);
  remaining -= textures.size();

  for (auto& result : textures) {
    _finalize_texture(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto meshes = _loader.take_resolved_meshes(remaining);
  remaining -= meshes.size();

  for (auto& result : meshes) {
    _finalize_mesh(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto fonts = _loader.take_resolved_fonts(remaining);
  remaining -= fonts.size();

  for (auto& result : fonts) {
    _finalize_font(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto materials = _loader.take_resolved_materials(remaining);
  remaining -= materials.size();

  for (auto& result : materials) {
    _finalize_material(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto particle_effects = _loader.take_resolved_particle_effects(remaining);
  remaining -= particle_effects.size();

  for (auto& result : particle_effects) {
    _finalize_particle_effect(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto animation_graphs = _loader.take_resolved_animation_graphs(remaining);
  remaining -= animation_graphs.size();

  for (auto& result : animation_graphs) {
    _finalize_animation_graph(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto shader_graphs = _loader.take_resolved_shader_graphs(remaining);
  remaining -= shader_graphs.size();

  for (auto& result : shader_graphs) {
    _finalize_shader_graph(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto skeletons = _loader.take_resolved_skeletons(remaining);
  remaining -= skeletons.size();

  for (auto& result : skeletons) {
    _finalize_skeleton(result);
  }

  if (remaining == 0u) {
    return;
  }

  auto animation_clips = _loader.take_resolved_animation_clips(remaining);

  for (auto& result : animation_clips) {
    _finalize_animation_clip(result);
  }
}

auto asset_residency::_finalize_texture(asset_loader::texture_result& result) -> void {
  const auto& request = result.request;

  if (result.did_cook) {
    _manifest.record_cook(request.id, texture_cook_version, request.source);
  }

  if (!result.data) {
    utility::logger<"assets">::warn("Could not load cooked texture '{}'", request.cooked.generic_string());
    return;
  }

  const auto key = _texture_cache_key(request.id, request.format);

  auto lock = std::lock_guard{_mutex};

  const auto entry = _textures.find(key);

  if (entry == _textures.end()) {
    return; // defensive -- load_texture always inserts the placeholder before submitting
  }

  _pending_textures.push_back(pending_texture_upload{entry->second->index(), std::move(result.data->pixels), result.data->width, result.data->height, request.format});

  // Distinct from is_resident(): this means "decoded and queued," not "the GPU upload actually
  // landed" -- same relationship a font's is_loaded() (via its glyph table) has to its atlas
  // texture's separate is_resident().
  entry->second->_bump_generation();
}

auto asset_residency::_finalize_mesh(asset_loader::mesh_result& result) -> void {
  const auto& request = result.request;

  if (result.did_cook) {
    _manifest.record_cook(request.id, mesh_cooker_version, request.source);
  }

  if (!result.data) {
    utility::logger<"assets">::warn("Could not load cooked mesh '{}'", request.cooked.generic_string());
    return;
  }

  auto& data = *result.data;

  auto fallback_material = material_handle{};

  auto submeshes = std::vector<mesh::submesh>{};
  submeshes.reserve(data.submeshes.size());

  for (const auto& cooked_submesh : data.submeshes) {
    auto submesh_material = material_handle{};

    if (cooked_submesh.material != math::uuid::nil()) {
      submesh_material = request.options.extract_materials
        ? _extract_gltf_material(cooked_submesh.material, request.source)
        : load_material(cooked_submesh.material);
    }

    if (!submesh_material.is_valid()) {
      if (!fallback_material.is_valid()) {
        fallback_material = create_material(material::create_info{ .albedo = _magenta });
      }
      submesh_material = fallback_material;
    }

    auto lods = std::vector<mesh::lod_level>{};
    lods.reserve(cooked_submesh.lods.size());

    for (const auto& lod : cooked_submesh.lods) {
      lods.push_back(mesh::lod_level{lod.index_offset, lod.index_count, lod.error});
    }

    submeshes.push_back(mesh::submesh{cooked_submesh.index_offset, cooked_submesh.index_count, cooked_submesh.bounds, submesh_material, std::move(lods)});
  }

  const auto vertex_count = data.vertices.size();
  const auto index_count = data.indices.size();
  const auto submesh_count = submeshes.size();
  const auto has_skin_data = !data.skin_vertices.empty();

  auto record = std::shared_ptr<mesh>{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _meshes.find(request.id);
    if (entry == _meshes.end()) {
      return; // defensive -- load_mesh always inserts the placeholder before submitting
    }
    record = entry->second;
  }

  record->_finalize_content(std::move(submeshes), data.bounds, static_cast<std::uint32_t>(vertex_count));

  if (has_skin_data) {
    auto skeleton_handle_value = load_skeleton(data.skeleton);

    auto animation_clip_handles = std::vector<animation_clip_handle>{};
    animation_clip_handles.reserve(data.animation_clips.size());

    for (const auto& clip_id : data.animation_clips) {
      animation_clip_handles.push_back(load_animation_clip(clip_id));
    }

    record->_set_skeletal_data(std::move(skeleton_handle_value), std::move(animation_clip_handles));
  }

  // Bumped here, once, after *both* _finalize_content and (for a skinned mesh) _set_skeletal_data
  // have run -- so is_loaded() only ever reports true once skeleton/animation clips are set too,
  // not partway through. mesh doesn't bump this itself inside _finalize_content, unlike the other
  // placeholder-content types, for exactly this reason.
  record->_bump_generation();

  {
    auto lock = std::lock_guard{_mutex};
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(data.vertices), std::move(data.indices), std::move(data.skin_vertices)});
  }

  utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes{}", request.source.generic_string(), vertex_count, index_count, submesh_count, has_skin_data ? " (skinned)" : "");
}

auto asset_residency::_finalize_font(asset_loader::font_result& result) -> void {
  const auto& request = result.request;

  if (result.did_cook) {
    _manifest.record_cook(request.id, font_cook_version, request.source);
  }

  if (!result.data) {
    utility::logger<"assets">::warn("Could not load cooked font '{}'", request.cooked.generic_string());
    return;
  }

  auto& data = *result.data;

  auto lock = std::lock_guard{_mutex};

  const auto entry = _fonts.find(request.id);

  if (entry == _fonts.end()) {
    return; // defensive -- load_font always inserts the placeholder before submitting
  }

  auto& record = *entry->second;

  record._finalize_content(std::move(data.glyphs), data.first_codepoint, data.line_height, data.ascent, data.descent);

  _pending_textures.push_back(pending_texture_upload{record.atlas()->index(), std::move(data.atlas.pixels), data.atlas.width, data.atlas.height, graphics::format::r8_unorm});
}

auto asset_residency::_finalize_material(asset_loader::material_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Unknown material uuid {}", request.id);
    return;
  }

  const auto& description = *result.data;

  auto info = material::create_info{};
  info.name = description.name;
  info.base_color_factor = description.base_color_factor;
  info.emissive_factor = description.emissive_factor;
  info.metallic_factor = description.metallic_factor;
  info.roughness_factor = description.roughness_factor;
  info.alpha = description.alpha;
  info.shading = description.shading;
  info.alpha_cutoff = description.alpha_cutoff;
  info.is_double_sided = description.is_double_sided;
  info.casts_shadow = description.casts_shadow;
  info.receives_shadow = description.receives_shadow;
  info.normal_scale = description.normal_scale;
  info.occlusion_strength = description.occlusion_strength;
  info.emissive_strength = description.emissive_strength;
  info.ior = description.ior;
  info.uv_tiling = description.uv_tiling;
  info.uv_offset = description.uv_offset;

  const auto load_slot = [this](const std::string& path, graphics::format format) -> texture_handle {
    return path.empty() ? texture_handle{} : load_texture(std::filesystem::path{path}, format);
  };

  info.albedo = load_slot(description.albedo, graphics::format::r8g8b8a8_srgb);
  info.normal = load_slot(description.normal, graphics::format::r8g8b8a8_unorm);
  info.metallic_roughness = load_slot(description.metallic_roughness, graphics::format::r8g8b8a8_unorm);
  info.occlusion = load_slot(description.occlusion, graphics::format::r8g8b8a8_unorm);
  info.emissive = load_slot(description.emissive, graphics::format::r8g8b8a8_srgb);

  info.shader_graph = description.shader_graph.empty() ? shader_graph_handle{} : load_shader_graph(std::filesystem::path{description.shader_graph});
  info.generic_params = description.generic_params;

  for (auto i = std::size_t{0u}; i < description.generic_texture_paths.size(); ++i) {
    info.generic_textures[i] = load_slot(description.generic_texture_paths[i], graphics::format::r8g8b8a8_srgb);
  }

  auto handle = material_handle{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _material_files.find(request.id);
    if (entry == _material_files.end()) {
      return; // defensive -- load_material always inserts the placeholder before submitting
    }
    handle = material_handle{entry->second};
  }

  update_material(handle, info);

  utility::logger<"assets">::info("Loaded material '{}'", request.source.empty() ? fmt::format("{}", request.id) : request.source.generic_string());
}

auto asset_residency::_finalize_particle_effect(asset_loader::particle_effect_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Could not parse particle_effect '{}'", request.source.generic_string());
    return;
  }

  const auto& description = *result.data;

  auto info = particle_effect::create_info{};
  info.name = description.name;
  info.emitters.reserve(description.emitters.size());

  for (const auto& emitter_description : description.emitters) {
    auto emitter = particle_emitter{};

    emitter.name = emitter_description.name;
    emitter.simulation_mode = emitter_description.simulation_mode;
    emitter.blend_mode = emitter_description.blend_mode;
    emitter.emission_rate = emitter_description.emission_rate;
    emitter.burst_count = emitter_description.burst_count;
    emitter.shape = emitter_description.shape;
    emitter.shape_extents = emitter_description.shape_extents;
    emitter.cone = emitter_description.cone;
    emitter.velocity_min = emitter_description.velocity_min;
    emitter.velocity_max = emitter_description.velocity_max;
    emitter.lifetime_min = emitter_description.lifetime_min;
    emitter.lifetime_max = emitter_description.lifetime_max;
    emitter.start_color = emitter_description.start_color;
    emitter.end_color = emitter_description.end_color;
    emitter.color_over_lifetime = emitter_description.color_over_lifetime;
    emitter.size_min = emitter_description.size_min;
    emitter.size_max = emitter_description.size_max;
    emitter.size_over_lifetime = emitter_description.size_over_lifetime;
    emitter.rotation_min = emitter_description.rotation_min;
    emitter.rotation_max = emitter_description.rotation_max;
    emitter.rotation_over_lifetime = emitter_description.rotation_over_lifetime;
    emitter.velocity_over_lifetime = emitter_description.velocity_over_lifetime;
    emitter.force_over_lifetime_min = emitter_description.force_over_lifetime_min;
    emitter.force_over_lifetime_max = emitter_description.force_over_lifetime_max;
    emitter.gravity = emitter_description.gravity;
    emitter.drag = emitter_description.drag;

    if (!emitter_description.texture.empty()) {
      emitter.texture = load_texture(std::filesystem::path{emitter_description.texture}, graphics::format::r8g8b8a8_srgb);
    }

    emitter.render_mode = emitter_description.render_mode;

    if (!emitter_description.render_mesh.empty()) {
      emitter.render_mesh = load_mesh(std::filesystem::path{emitter_description.render_mesh});
    }

    if (!emitter_description.render_material.empty()) {
      emitter.render_material = load_material(std::filesystem::path{emitter_description.render_material});
    }

    emitter.collision = emitter_description.collision;

    for (const auto& binding_description : emitter_description.sub_emitters) {
      auto binding = sub_emitter_binding{};
      binding.event = binding_description.event;

      if (!binding_description.effect.empty()) {
        binding.effect = load_particle_effect(std::filesystem::path{binding_description.effect});
      }

      binding.probability = binding_description.probability;
      binding.inherit_velocity = binding_description.inherit_velocity;

      emitter.sub_emitters.push_back(binding);
    }

    emitter.trail = emitter_description.trail;

    info.emitters.push_back(std::move(emitter));
  }

  auto handle = particle_effect_handle{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _particle_effect_files.find(request.id);
    if (entry == _particle_effect_files.end()) {
      return; // defensive -- load_particle_effect always inserts the placeholder before submitting
    }
    handle = particle_effect_handle{entry->second};
  }

  update_particle_effect(handle, info);

  utility::logger<"assets">::info("Loaded particle_effect '{}'", request.source.generic_string());
}

auto asset_residency::_finalize_animation_graph(asset_loader::animation_graph_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Could not parse animation_graph '{}'", request.source.generic_string());
    return;
  }

  auto handle = animation_graph_handle{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _animation_graph_files.find(request.id);
    if (entry == _animation_graph_files.end()) {
      return; // defensive -- load_animation_graph always inserts the placeholder before submitting
    }
    handle = animation_graph_handle{entry->second};
  }

  update_animation_graph(handle, *result.data);

  utility::logger<"assets">::info("Loaded animation_graph '{}'", request.source.generic_string());
}

auto asset_residency::_finalize_shader_graph(asset_loader::shader_graph_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Could not parse shader_graph '{}'", request.source.generic_string());
    return;
  }

  const auto& description = *result.data;

  auto info = shader_graph::create_info{};
  info.name = description.name;
  info.edges = description.edges;

  info.nodes.reserve(description.nodes.size());

  for (const auto& node_description : description.nodes) {
    auto node = shader_graph_node{};
    node.id = node_description.id;
    node.type = node_description.type;
    node.editor_position = node_description.editor_position;
    node.name = node_description.name;
    node.exposed = node_description.exposed;
    node.preview = node_description.preview;

    if (const auto* pattern = std::get_if<std::string>(&node_description.value); pattern && node_description.type == shader_node_type::swizzle) {
      node.value = *pattern;
    } else if (const auto* path = std::get_if<std::string>(&node_description.value)) {
      node.value = path->empty() ? texture_handle{} : load_texture(std::filesystem::path{*path}, graphics::format::r8g8b8a8_srgb);
    } else if (const auto* value = std::get_if<std::float_t>(&node_description.value)) {
      node.value = *value;
    } else if (const auto* value = std::get_if<math::vector2>(&node_description.value)) {
      node.value = *value;
    } else if (const auto* value = std::get_if<math::vector3>(&node_description.value)) {
      node.value = *value;
    } else if (const auto* value = std::get_if<math::vector4>(&node_description.value)) {
      node.value = *value;
    } else if (const auto* value = std::get_if<math::color>(&node_description.value)) {
      node.value = *value;
    }
    // monostate (math/input/output nodes) -- default-constructed monostate, nothing to resolve

    info.nodes.push_back(node);
  }

  auto handle = shader_graph_handle{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _shader_graph_files.find(request.id);
    if (entry == _shader_graph_files.end()) {
      return; // defensive -- load_shader_graph always inserts the placeholder before submitting
    }
    handle = shader_graph_handle{entry->second};
  }

  // update_shader_graph itself cooks now (see its own doc comment) -- no separate cook_shader_graph
  // call needed here anymore.
  update_shader_graph(handle, info);

  utility::logger<"assets">::info("Loaded shader_graph '{}'", request.source.generic_string());
}

auto asset_residency::_finalize_skeleton(asset_loader::skeleton_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Could not load skeleton {}", request.id);
    return;
  }

  auto lock = std::lock_guard{_mutex};

  const auto entry = _skeletons.find(request.id);

  if (entry == _skeletons.end()) {
    return; // defensive -- load_skeleton always inserts the placeholder before submitting
  }

  entry->second->_finalize_content(std::move(*result.data));
}

auto asset_residency::_finalize_animation_clip(asset_loader::animation_clip_result& result) -> void {
  const auto& request = result.request;

  if (!result.data) {
    utility::logger<"assets">::warn("Could not load animation clip {}", request.id);
    return;
  }

  auto& data = *result.data;

  auto lock = std::lock_guard{_mutex};

  const auto entry = _animation_clips.find(request.id);

  if (entry == _animation_clips.end()) {
    return; // defensive -- load_animation_clip always inserts the placeholder before submitting
  }

  entry->second->_finalize_content(std::move(data.name), data.duration, std::move(data.channels));
}

auto asset_residency::process_uploads(std::uint64_t frame_index) -> void {
  SBX_PROFILE_SCOPE("asset_residency::process_uploads");

  _drain_loader_results();

  auto pending_textures = std::vector<pending_texture_upload>{};
  auto pending_meshes = std::vector<pending_mesh_upload>{};
  auto pending_materials = std::vector<pending_material_upload>{};

  {
    auto lock = std::lock_guard{_mutex};

    auto budget = max_uploads_per_frame;

    while (budget > 0u && !_pending_textures.empty()) {
      pending_textures.push_back(std::move(_pending_textures.front()));
      _pending_textures.pop_front();
      --budget;
    }

    while (budget > 0u && !_pending_meshes.empty()) {
      pending_meshes.push_back(std::move(_pending_meshes.front()));
      _pending_meshes.pop_front();
      --budget;
    }

    while (budget > 0u && !_pending_materials.empty()) {
      pending_materials.push_back(std::move(_pending_materials.front()));
      _pending_materials.pop_front();
      --budget;
    }
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
    const auto vertex_bytes = graphics::buffer::size_type{request.vertices.size() * sizeof(vertex)};
    const auto index_bytes = graphics::buffer::size_type{request.indices.size() * sizeof(std::uint32_t)};

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
      const auto skin_vertex_bytes = graphics::buffer::size_type{request.skin_vertices.size() * sizeof(skin_vertex)};

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
    data.uv_tiling = material.uv_tiling();
    data.uv_offset = material.uv_offset();

    const auto& generic_params = material.generic_params();
    std::ranges::copy(generic_params, data.generic_params);

    const auto& generic_textures = material.generic_textures();
    for (auto i = std::size_t{0u}; i < generic_textures.size(); ++i) {
      data.generic_textures[i] = resolve(generic_textures[i], _white);
    }

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

auto asset_residency::is_resident(const font_handle& font) const -> bool {
  return font.is_valid() && is_resident(font->atlas());
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

auto asset_residency::_extract_gltf_material(const math::uuid& cooked_material_id, const std::filesystem::path& mesh_source) -> material_handle {
  const auto description = asset_cooker::resolve_cooked_material(cooked_material_id);

  if (!description) {
    return material_handle{};
  }

  // mesh_source is fully resolved; save_material/load_material(path) expect an assets-relative
  // input, so re-relativize it here (same as the editor's extract_material_to_asset).
  const auto source_relative = _manifest.relative(mesh_source);

  const auto directory = source_relative.parent_path() / "materials"; // mirrors textures already landing in models/<name>/textures/
  const auto relative_path = directory / (sanitize_file_name(description->name.empty() ? "material" : description->name) + ".material");

  // Already extracted (possibly hand-edited since a previous cook, or since the last time this
  // mesh was loaded) — reuse it as-is, never overwrite.
  if (std::filesystem::exists(_manifest.absolute(relative_path))) {
    if (auto existing = load_material(relative_path); existing.is_valid()) {
      return existing;
    }
  }

  auto info = material::create_info{};
  info.name = description->name.empty() ? "material" : description->name;
  info.base_color_factor = description->base_color_factor;
  info.emissive_factor = description->emissive_factor;
  info.metallic_factor = description->metallic_factor;
  info.roughness_factor = description->roughness_factor;
  info.alpha = description->alpha;
  info.alpha_cutoff = description->alpha_cutoff;
  info.is_double_sided = description->is_double_sided;
  info.casts_shadow = description->casts_shadow;
  info.receives_shadow = description->receives_shadow;

  const auto load_slot = [this](const std::string& path, graphics::format format) -> texture_handle {
    return path.empty() ? texture_handle{} : load_texture(std::filesystem::path{path}, format);
  };

  info.albedo = load_slot(description->albedo, graphics::format::r8g8b8a8_srgb);
  info.normal = load_slot(description->normal, graphics::format::r8g8b8a8_unorm);
  info.metallic_roughness = load_slot(description->metallic_roughness, graphics::format::r8g8b8a8_unorm);
  info.occlusion = load_slot(description->occlusion, graphics::format::r8g8b8a8_unorm);
  info.emissive = load_slot(description->emissive, graphics::format::r8g8b8a8_srgb);

  auto handle = create_material(info);

  save_material(handle, relative_path);

  return handle;
}

} // namespace sbx::assets
