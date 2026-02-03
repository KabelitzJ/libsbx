// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PARTICLES_PARTICLE_EMITTER_HPP_
#define LIBSBX_PARTICLES_PARTICLE_EMITTER_HPP_

#include <cstdint>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::particles {

enum class emitter_state : std::uint8_t {
  playing,
  paused,
  stopped
}; // enum class emitter_state

struct particle_emitter {
  emitter_state state{emitter_state::playing};
  bool loop{true};

  std::float_t duration{5.0f};
  std::float_t elapsed{0.0f};
  std::float_t emission_accumulator{0.0f};

  std::uint32_t max_particles{10000};
  std::float_t emission_rate{100.0f};
  math::vector3 emission_shape_min{-1.0f, 0.0f, -1.0f};
  math::vector3 emission_shape_max{1.0f, 0.0f, 1.0f};
  math::vector2 initial_speed{5.0f, 10.0f};
  math::vector2 initial_lifetime{1.0f, 3.0f};
  math::vector2 initial_size{0.1f, 0.5f};
  math::color initial_color{1.0f, 1.0f, 1.0f, 1.0f};

  math::vector3 gravity{0.0f, -9.81f, 0.0f};
  std::float_t drag{0.1f};

  math::color end_color{1.0f, 1.0f, 1.0f, 0.0f};
  std::float_t end_size_scale{0.0f};

  graphics::image2d_handle texture{};

  std::uint32_t burst_count{0};

  auto play() -> void {
    state = emitter_state::playing;
  }

  auto pause() -> void {
    state = emitter_state::paused;
  }

  auto stop() -> void {
    state = emitter_state::stopped;
    elapsed = 0.0f;
    emission_accumulator = 0.0f;
  }

  auto burst(std::uint32_t count) -> void {
    burst_count += count;
  }

  [[nodiscard]] auto is_playing() const -> bool {
    return state == emitter_state::playing;
  }

  [[nodiscard]] auto is_paused() const -> bool {
    return state == emitter_state::paused;
  }

  [[nodiscard]] auto is_stopped() const -> bool {
    return state == emitter_state::stopped;
  }

  [[nodiscard]] auto is_finished() const -> bool {
    return !loop && elapsed >= duration && state == emitter_state::stopped;
  }

}; // struct particle_emitter

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLE_EMITTER_HPP_