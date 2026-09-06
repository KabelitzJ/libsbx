// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/particles/particles_module.hpp>

#include <algorithm>

#include <libsbx/math/algorithm.hpp>

#include <libsbx/particles/spawn.hpp>

namespace sbx::particles {

// Bounds how many child nodes one emitter's sub-emitters can ever occupy, so a high-frequency event
// (birth at a fast emission rate, or collision) can't churn the scene's node count unbounded --
// pooled/reused nodes (see _trigger_sub_emitters) are the common case once this fills up.
inline constexpr auto sub_emitter_pool_capacity = std::size_t{32};

[[nodiscard]] auto lerp_color(const math::color& start, const math::color& end, std::float_t t) -> math::color {
  return math::color{
    math::mix(start.r(), end.r(), t),
    math::mix(start.g(), end.g(), t),
    math::mix(start.b(), end.b(), t),
    math::mix(start.a(), end.a(), t)
  };
}

auto particles_module::fixed_update() -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  // Mirrors physics_module::fixed_update()'s gate: particles freeze in edit mode, same as bodies do,
  // so "world" collision (which reads physics_module's broadphase, also empty outside Play) and
  // particle motion stay consistent with each other.
  if (!scenes_module.is_simulating()) {
    return;
  }

  auto& scene = scenes_module.active_scene();
  const auto dt = core::engine::fixed_delta_time().value();

  for (auto&& [entity, world, effect] : scene.query<scenes::world_transform, scenes::particle_effect>().each()) {
    _simulate_effect(scene, effect, world.matrix, dt);
  }
}

auto particles_module::_simulate_effect(scenes::scene& scene, scenes::particle_effect& effect, const math::matrix4x4& world, std::float_t dt) -> void {
  if (effect.playback == scenes::particle_playback_state::stopped) {
    for (auto& runtime : effect.emitters) {
      runtime.particles.clear();
      runtime.trails.clear();
      runtime.emission_accumulator = 0.0f;
      runtime.burst_fired = false;
      // GPU-path only: mark the pool slot unclaimed so scene_renderer_module's extraction
      // stops calling keep_alive on it and the pool's own tick() drains it -- no explicit release
      // call needed, this mirrors how the CPU path's runtime.particles.clear() above works.
      runtime.slot = scenes::particle_emitter::invalid_slot;
    }

    effect.elapsed = 0.0f;

    return;
  }

  if (!effect.effect.is_valid()) {
    return;
  }

  // The runtime vector is index-paired with the asset's emitters(); keep it in step whenever the
  // effect handle changes or emitters are added/removed in the editor.
  if (effect.emitters.size() != effect.effect->emitters().size()) {
    effect.emitters.resize(effect.effect->emitters().size());
  }

  if (effect.playback == scenes::particle_playback_state::paused) {
    return;
  }

  effect.elapsed += dt;

  if (effect.loop && effect.elapsed >= effect.duration) {
    effect.elapsed -= effect.duration;

    for (auto& runtime : effect.emitters) {
      runtime.burst_fired = false;
    }
  }

  const auto emitting = effect.loop || effect.elapsed < effect.duration;

  const auto& configs = effect.effect->emitters();

  for (auto index = std::size_t{0u}; index < configs.size(); ++index) {
    const auto& config = configs[index];
    auto& runtime = effect.emitters[index];

    if (config.simulation_mode == assets::particle_simulation_mode::gpu) {
      // GPU path (render/particles/particle_simulate_pass.hpp): scene_renderer_module's extraction loop
      // owns this emitter's slot/keep_alive/tick and emission timing instead of this loop. runtime.
      // particles stays empty, so playback can flip to stopped before the GPU pool has actually drained -- harmless.
      continue;
    }

    _spawn(scene, config, runtime, world, effect.inherited_velocity, emitting, dt);
    _integrate(scene, config, runtime, dt);

    if (config.collision.mode != assets::particle_collision_mode::none) {
      _resolve_collisions(scene, config, runtime);
    }

    if (config.trail.enabled) {
      _record_trails(config, runtime, dt);
    } else if (!runtime.trails.empty()) {
      runtime.trails.clear();
    }
  }

  if (!effect.loop && effect.elapsed >= effect.duration) {
    const auto all_empty = std::ranges::all_of(effect.emitters, [](const auto& runtime) { return runtime.particles.empty(); });

    if (all_empty) {
      effect.playback = scenes::particle_playback_state::stopped;
    }
  }
}

auto particles_module::_spawn(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const math::matrix4x4& world, const math::vector3& inherited_velocity, bool emitting, std::float_t dt) -> void {
  if (!emitting) {
    return;
  }

  const auto spawn_one = [&] {
    auto spawned = roll_particle(config, world, runtime.next_particle_id++, inherited_velocity);
    _trigger_sub_emitters(scene, runtime, config, assets::sub_emitter_event::birth, spawned);
    runtime.particles.push_back(spawned);
  };

  runtime.emission_accumulator += config.emission_rate * dt;

  while (runtime.emission_accumulator >= 1.0f) {
    runtime.emission_accumulator -= 1.0f;
    spawn_one();
  }

  if (config.burst_count > 0u && !runtime.burst_fired) {
    for (auto i = std::uint32_t{0u}; i < config.burst_count; ++i) {
      spawn_one();
    }

    runtime.burst_fired = true;
  }
}

auto particles_module::_integrate(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void {
  for (auto& p : runtime.particles) {
    p.age += dt;

    const auto t = p.lifetime > 0.0f ? std::clamp(p.age / p.lifetime, 0.0f, 1.0f) : 1.0f;

    p.velocity += (math::vector3{0.0f, -config.gravity, 0.0f} + p.constant_force) * dt;
    p.velocity *= std::max(0.0f, 1.0f - config.drag * dt);

    // velocity_over_lifetime is an additive world-space layer on top of the integrated velocity, not
    // itself integrated -- matches Unity's "Velocity over Lifetime" module, which re-evaluates fresh
    // every step rather than accumulating.
    const auto effective_velocity = config.velocity_over_lifetime.has_keys() ? p.velocity + config.velocity_over_lifetime.evaluate(t) : p.velocity;

    p.previous_position = p.position;
    p.position += effective_velocity * dt;

    if (config.rotation_over_lifetime.has_keys()) {
      p.rotation += config.rotation_over_lifetime.evaluate(t) * dt;
    }

    p.color = config.color_over_lifetime.has_keys() ? config.color_over_lifetime.evaluate(t) : lerp_color(config.start_color, config.end_color, t);
    p.size = p.base_size * (config.size_over_lifetime.has_keys() ? config.size_over_lifetime.evaluate(t) : 1.0f);
  }

  // death sub-emitters must see the particle's final state, so fire them before erasing.
  for (const auto& p : runtime.particles) {
    if (p.age >= p.lifetime) {
      _trigger_sub_emitters(scene, runtime, config, assets::sub_emitter_event::death, p);
    }
  }

  std::erase_if(runtime.particles, [](const particle& p) { return p.age >= p.lifetime; });
}

auto particles_module::_resolve_collisions(scenes::scene& scene, const assets::particle_emitter& config, scenes::particle_emitter& runtime) -> void {
  const auto& collision = config.collision;

  const auto respond = [&](particle& p, const math::vector3& normal, std::float_t push_out) {
    p.position += normal * push_out;
    p.velocity = math::vector3::reflect(p.velocity, normal) * collision.bounce;
    p.velocity *= (1.0f - collision.dampen);
    p.lifetime -= p.lifetime * collision.lifetime_loss;
    p.collision_count += 1u;

    _trigger_sub_emitters(scene, runtime, config, assets::sub_emitter_event::collision, p);

    if (collision.max_collisions_per_particle > 0u && p.collision_count >= collision.max_collisions_per_particle) {
      p.age = p.lifetime;
    }
  };

  if (collision.mode == assets::particle_collision_mode::planes) {
    for (auto& p : runtime.particles) {
      const auto radius = p.size * collision.radius_scale;

      for (const auto& plane : collision.planes) {
        const auto distance = math::vector3::dot(plane.normal, p.position) - plane.distance;

        if (distance < radius) {
          respond(p, plane.normal, radius - distance);
        }
      }
    }

    return;
  }

  auto& physics_module = core::engine::get_module<physics::physics_module>();
  auto hits = std::vector<physics::sphere_query_hit>{};

  for (auto& p : runtime.particles) {
    const auto radius = p.size * collision.radius_scale;

    physics_module.query_sphere_contacts(scene, p.position, radius, hits);

    for (const auto& hit : hits) {
      respond(p, hit.normal, hit.penetration_depth);
    }
  }
}

// static_vector has no push_front()/erase() -- points.front() is the tail (oldest), points.back()
// the head (newest); once full, the oldest is dropped by shifting everyone down one slot and
// overwriting the last (a plain push_back would just fail/no-op once at capacity).
auto push_trail_point(trail& trail, const trail_point& point) -> void {
  if (trail.points.is_full()) {
    for (auto i = std::size_t{0u}; i + 1u < trail.points.size(); ++i) {
      trail.points[i] = trail.points[i + 1u];
    }

    trail.points[trail.points.size() - 1u] = point;
  } else {
    trail.points.push_back(point);
  }
}

auto particles_module::_record_trails(const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void {
  for (auto& p : runtime.particles) {
    auto* existing = static_cast<trail*>(nullptr);

    for (auto& candidate : runtime.trails) {
      if (candidate.particle_id == p.id) {
        existing = &candidate;
        break;
      }
    }

    if (existing == nullptr) {
      runtime.trails.push_back(trail{p.id, true, {}});
      existing = &runtime.trails.back();
    }

    const auto should_record = existing->points.is_empty() || (existing->points.back().position - p.position).length() >= config.trail.min_vertex_distance;

    if (should_record) {
      push_trail_point(*existing, trail_point{p.position, p.color, 0.0f});
    }
  }

  for (auto& trail_it : runtime.trails) {
    for (auto& point : trail_it.points) {
      point.age += dt;
    }
  }

  // Drop expired points by compaction -- static_vector has no erase(), so surviving points are
  // shifted down to fill the gap and the tail is trimmed with pop_back().
  for (auto& trail_it : runtime.trails) {
    auto write_index = std::size_t{0u};

    for (auto read_index = std::size_t{0u}; read_index < trail_it.points.size(); ++read_index) {
      if (trail_it.points[read_index].age < config.trail.lifetime) {
        trail_it.points[write_index++] = trail_it.points[read_index];
      }
    }

    while (trail_it.points.size() > write_index) {
      trail_it.points.pop_back();
    }
  }

  for (auto index = runtime.trails.size(); index-- > 0;) {
    auto& trail_it = runtime.trails[index];

    if (trail_it.particle_alive) {
      const auto particle_exists = std::ranges::any_of(runtime.particles, [&](const auto& p) { return p.id == trail_it.particle_id; });

      if (!particle_exists) {
        trail_it.particle_alive = false;

        if (config.trail.die_with_particle) {
          trail_it.points.clear();
        }
      }
    }

    if (trail_it.points.is_empty()) {
      // Fully faded (or forcibly cleared) -- order doesn't matter, swap-and-pop.
      trail_it = runtime.trails.back();
      runtime.trails.pop_back();
    }
  }
}

auto particles_module::_trigger_sub_emitters(scenes::scene& scene, scenes::particle_emitter& runtime, const assets::particle_emitter& config, assets::sub_emitter_event event, const particle& p) -> void {
  for (const auto& binding : config.sub_emitters) {
    if (binding.event != event || !binding.effect.is_valid()) {
      continue;
    }

    if (math::random::next<std::float_t>(0.0f, 1.0f) > binding.probability) {
      continue;
    }

    auto child = scenes::node{};

    for (const auto& id : runtime.sub_emitter_pool) {
      auto candidate = scene.find(id);

      if (candidate.is_valid() && candidate.has_component<scenes::particle_effect>() && candidate.get_component<scenes::particle_effect>().playback == scenes::particle_playback_state::stopped) {
        child = candidate;
        break;
      }
    }

    if (!child.is_valid()) {
      if (runtime.sub_emitter_pool.size() >= sub_emitter_pool_capacity) {
        continue; // pool exhausted -- skip this spawn rather than growing the scene unbounded
      }

      child = scene.create_node("Sub Emitter", scenes::local_transform{.position = p.position});
      runtime.sub_emitter_pool.push_back(child.id());
    } else {
      child.transform().position = p.position;
    }

    // world_transform isn't recomputed until scenes_module::late_update(); write it directly here too,
    // so a freshly (re)positioned sub-emitter's particles spawn at the right place this same frame.
    child.get_component<scenes::world_transform>().matrix = math::matrix4x4::translated(math::matrix4x4::identity, p.position);

    auto& child_effect = child.get_or_add_component<scenes::particle_effect>();
    child_effect.effect = binding.effect;
    child_effect.loop = false;
    child_effect.elapsed = 0.0f;
    child_effect.playback = scenes::particle_playback_state::playing;
    child_effect.emitters.clear();
    child_effect.inherited_velocity = binding.inherit_velocity ? p.velocity : math::vector3{0.0f, 0.0f, 0.0f};
  }
}

} // namespace sbx::particles
