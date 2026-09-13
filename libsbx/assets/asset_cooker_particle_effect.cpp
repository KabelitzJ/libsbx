// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

auto asset_cooker::parse_particle_effect_file(const std::filesystem::path& source) -> std::optional<particle_effect_description> {
  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(source.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse particle_effect '{}' ({})", source.generic_string(), exception.what());
    return std::nullopt;
  }

  auto description = particle_effect_description{};

  if (root["name"]) description.name = root["name"].as<std::string>();

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
    description.emitters.reserve(emitters.size());

    for (const auto emitter_node : emitters) {
      auto emitter = particle_emitter_description{};

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
        emitter.texture = emitter_node["texture"].as<std::string>();
      }

      if (emitter_node["render_mode"]) {
        emitter.render_mode = (emitter_node["render_mode"].as<std::string>() == "mesh") ? particle_render_mode::mesh : particle_render_mode::billboard;
      }

      if (emitter_node["render_mesh"]) {
        emitter.render_mesh = emitter_node["render_mesh"].as<std::string>();
      }

      if (emitter_node["render_material"]) {
        emitter.render_material = emitter_node["render_material"].as<std::string>();
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
          auto binding = particle_emitter_description::sub_emitter_description{};

          if (binding_node["event"]) {
            const auto event = binding_node["event"].as<std::string>();
            binding.event = (event == "death") ? sub_emitter_event::death : (event == "collision") ? sub_emitter_event::collision : sub_emitter_event::birth;
          }

          if (binding_node["effect"]) {
            binding.effect = binding_node["effect"].as<std::string>();
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

      description.emitters.push_back(std::move(emitter));
    }
  }

  return description;
}


} // namespace sbx::assets
