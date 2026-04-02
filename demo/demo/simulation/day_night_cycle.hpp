// SPDX-License-Identifier: MIT
#ifndef DEMO_SIMULATION_DAY_NIGHT_CYCLE_HPP_
#define DEMO_SIMULATION_DAY_NIGHT_CYCLE_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace demo {

struct sun_state {
  // Normalized direction toward the sun (world space, y-up)
  std::float_t direction_x;
  std::float_t direction_y;
  std::float_t direction_z;

  // Sun disc color (linear RGB)
  std::float_t sun_r;
  std::float_t sun_g;
  std::float_t sun_b;

  // Direct sunlight intensity multiplier
  std::float_t sun_intensity;

  // Ambient color (linear RGB)
  std::float_t ambient_r;
  std::float_t ambient_g;
  std::float_t ambient_b;

  // Ambient intensity multiplier
  std::float_t ambient_intensity;

  // 0 = full night, 1 = full day
  std::float_t daylight_factor;

}; // struct sun_state

struct day_night_params {
  // Sun orbit tilt from vertical (latitude feel). 0 = equator, higher = more angled.
  std::float_t orbit_tilt{0.4f};

  // Compass angle of the sun's orbital plane (radians). 0 = E-W, pi/4 = NE-SW.
  std::float_t orbit_rotation{0.2f};

  // Peak sun intensity at noon
  std::float_t peak_intensity{1.2f};

  // Minimum ambient at night
  std::float_t night_ambient{0.03f};

  // Maximum ambient at noon
  std::float_t day_ambient{0.15f};

  // Hour of sunrise center (sun crosses horizon)
  std::float_t sunrise_hour{6.0f};

  // Hour of sunset center
  std::float_t sunset_hour{18.0f};

  // Duration of twilight transition (hours, each side of sunrise/sunset)
  std::float_t twilight_duration{1.5f};

}; // struct day_night_params

inline auto compute_sun_state(std::float_t continuous_hour, const day_night_params& params = {}) -> sun_state {
  auto state = sun_state{};

  constexpr auto pi = 3.14159265f;
  constexpr auto two_pi = pi * 2.0f;

  // Sun angle: 0 at midnight, pi at noon, 2pi back to midnight
  auto sun_angle = (continuous_hour / 24.0f) * two_pi - (pi * 0.5f);

  // Sun position in the orbital plane (before tilt/rotation)
  auto orbit_x = std::cos(sun_angle);
  auto orbit_y = std::sin(sun_angle);

  // Apply orbit tilt (how high the sun gets)
  auto tilt = params.orbit_tilt;
  auto sun_y = orbit_y * std::cos(tilt);
  auto forward = orbit_y * std::sin(tilt);

  // Rotate the horizontal component by orbit_rotation
  auto rot = params.orbit_rotation;
  auto sun_x = orbit_x * std::cos(rot) - forward * std::sin(rot);
  auto sun_z = orbit_x * std::sin(rot) + forward * std::cos(rot);

  // Normalize direction (toward the sun)
  auto len = std::sqrt(sun_x * sun_x + sun_y * sun_y + sun_z * sun_z);

  if (len > 0.001f) {
    state.direction_x = sun_x / len;
    state.direction_y = sun_y / len;
    state.direction_z = sun_z / len;
  } else {
    state.direction_x = 0.0f;
    state.direction_y = 1.0f;
    state.direction_z = 0.0f;
  }

  // Elevation: how far above/below horizon. Negative = below.
  auto elevation = state.direction_y;

  // Daylight factor: 0 at night, 1 during day, smooth transition at twilight
  auto horizon_blend = std::clamp(elevation * (12.0f / params.twilight_duration) + 0.5f, 0.0f, 1.0f);
  auto daylight = horizon_blend * horizon_blend * (3.0f - 2.0f * horizon_blend);
  state.daylight_factor = daylight;

  // Sun color: white at noon, warm amber at horizon, gone at night
  auto horizon_factor = 1.0f - std::clamp(elevation * 4.0f, 0.0f, 1.0f);
  horizon_factor = std::max(horizon_factor, 0.0f);

  // Lerp white → amber based on how close to horizon
  auto noon_r = 1.0f;
  auto noon_g = 0.98f;
  auto noon_b = 0.95f;

  auto horizon_r = 1.0f;
  auto horizon_g = 0.55f;
  auto horizon_b = 0.2f;

  auto t = horizon_factor * horizon_factor;
  state.sun_r = std::lerp(noon_r, horizon_r, t);
  state.sun_g = std::lerp(noon_g, horizon_g, t);
  state.sun_b = std::lerp(noon_b, horizon_b, t);

  // Sun intensity: peaks at noon, zero when below horizon
  state.sun_intensity = daylight * params.peak_intensity;

  // Ambient: blue-tinted at night, neutral during day
  auto night_r = 0.1f;
  auto night_g = 0.12f;
  auto night_b = 0.2f;

  auto day_r = 0.6f;
  auto day_g = 0.65f;
  auto day_b = 0.7f;

  state.ambient_r = std::lerp(night_r, day_r, daylight);
  state.ambient_g = std::lerp(night_g, day_g, daylight);
  state.ambient_b = std::lerp(night_b, day_b, daylight);

  state.ambient_intensity = std::lerp(params.night_ambient, params.day_ambient, daylight);

  return state;
}

} // namespace demo

#endif // DEMO_SIMULATION_DAY_NIGHT_CYCLE_HPP_
