// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/particles/particles_module.hpp>

#include <algorithm>

#include <libsbx/math/algorithm.hpp>

#include <libsbx/particles/spawn.hpp>

namespace sbx::particles {

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
    _simulate_effect(effect, world.matrix, dt);
  }
}

auto particles_module::_simulate_effect(scenes::particle_effect& effect, const math::matrix4x4& world, std::float_t dt) -> void {
  if (effect.playback == scenes::particle_playback_state::stopped) {
    for (auto& runtime : effect.emitters) {
      runtime.particles.clear();
      runtime.emission_accumulator = 0.0f;
      runtime.burst_fired = false;
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
    _spawn(configs[index], effect.emitters[index], world, emitting, dt);
    _integrate(configs[index], effect.emitters[index], dt);
  }

  if (!effect.loop && effect.elapsed >= effect.duration) {
    const auto all_empty = std::ranges::all_of(effect.emitters, [](const auto& runtime) { return runtime.particles.empty(); });

    if (all_empty) {
      effect.playback = scenes::particle_playback_state::stopped;
    }
  }
}

auto particles_module::_spawn(const assets::particle_emitter& config, scenes::particle_emitter& runtime, const math::matrix4x4& world, bool emitting, std::float_t dt) -> void {
  if (!emitting) {
    return;
  }

  runtime.emission_accumulator += config.emission_rate * dt;

  while (runtime.emission_accumulator >= 1.0f) {
    runtime.emission_accumulator -= 1.0f;
    runtime.particles.push_back(roll_particle(config, world, runtime.next_particle_id++));
  }

  if (config.burst_count > 0u && !runtime.burst_fired) {
    for (auto i = std::uint32_t{0u}; i < config.burst_count; ++i) {
      runtime.particles.push_back(roll_particle(config, world, runtime.next_particle_id++));
    }

    runtime.burst_fired = true;
  }
}

auto particles_module::_integrate(const assets::particle_emitter& config, scenes::particle_emitter& runtime, std::float_t dt) -> void {
  for (auto& p : runtime.particles) {
    p.age += dt;

    const auto t = p.lifetime > 0.0f ? std::clamp(p.age / p.lifetime, 0.0f, 1.0f) : 1.0f;

    p.velocity += math::vector3{0.0f, -config.gravity, 0.0f} * dt;
    p.velocity *= std::max(0.0f, 1.0f - config.drag * dt);
    p.position += p.velocity * dt;

    p.color = lerp_color(config.start_color, config.end_color, t);
  }

  std::erase_if(runtime.particles, [](const particle& p) { return p.age >= p.lifetime; });
}

} // namespace sbx::particles
