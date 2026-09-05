// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/particle_effect.hpp>

#include <optional>

#include <libsbx/math/algorithm.hpp>

namespace sbx::assets {

/**
 * @brief Finds the pair of keys (by index into @p keys) bracketing @p t: the closest key at or
 * before it, and the closest key at or after it. Independent of authoring order -- a plain scan over
 * however many keys there are (capped at curve_max_keys/gradient_max_keys, so always cheap). Either
 * side is nullopt when @p t falls entirely before/after every key.
 */
template<typename Key, typename TimeOf>
[[nodiscard]] auto find_bracket(const Key* keys, std::size_t count, std::float_t t, TimeOf time_of) -> std::pair<std::optional<std::size_t>, std::optional<std::size_t>> {
  auto lower = std::optional<std::size_t>{};
  auto upper = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < count; ++index) {
    const auto key_time = time_of(keys[index]);

    if (key_time <= t && (!lower || key_time > time_of(keys[*lower]))) {
      lower = index;
    }

    if (key_time >= t && (!upper || key_time < time_of(keys[*upper]))) {
      upper = index;
    }
  }

  return {lower, upper};
}

auto curve::evaluate(std::float_t t) const -> std::float_t {
  if (keys.is_empty()) {
    return 0.0f;
  }

  const auto time_of = [](const curve_key& key) { return key.time; };
  const auto [lower, upper] = find_bracket(keys.data(), keys.size(), t, time_of);

  if (!lower) {
    return keys[*upper].value;
  }

  if (!upper || keys[*lower].time == keys[*upper].time) {
    return keys[*lower].value;
  }

  const auto local_t = (t - keys[*lower].time) / (keys[*upper].time - keys[*lower].time);

  return math::mix(keys[*lower].value, keys[*upper].value, local_t);
}

auto vector3_curve::evaluate(std::float_t t) const -> math::vector3 {
  return math::vector3{x.evaluate(t), y.evaluate(t), z.evaluate(t)};
}

auto gradient::evaluate(std::float_t t) const -> math::color {
  const auto color_time_of = [](const gradient_color_key& key) { return key.time; };
  const auto alpha_time_of = [](const gradient_alpha_key& key) { return key.time; };

  auto rgb = math::color{1.0f, 1.0f, 1.0f, 1.0f};

  if (!color_keys.is_empty()) {
    const auto [lower, upper] = find_bracket(color_keys.data(), color_keys.size(), t, color_time_of);

    if (!lower) {
      rgb = color_keys[*upper].color;
    } else if (!upper || color_keys[*lower].time == color_keys[*upper].time) {
      rgb = color_keys[*lower].color;
    } else {
      const auto& a = color_keys[*lower].color;
      const auto& b = color_keys[*upper].color;
      const auto local_t = (t - color_keys[*lower].time) / (color_keys[*upper].time - color_keys[*lower].time);

      rgb = math::color{math::mix(a.r(), b.r(), local_t), math::mix(a.g(), b.g(), local_t), math::mix(a.b(), b.b(), local_t), 1.0f};
    }
  }

  auto alpha = 1.0f;

  if (!alpha_keys.is_empty()) {
    const auto [lower, upper] = find_bracket(alpha_keys.data(), alpha_keys.size(), t, alpha_time_of);

    if (!lower) {
      alpha = alpha_keys[*upper].alpha;
    } else if (!upper || alpha_keys[*lower].time == alpha_keys[*upper].time) {
      alpha = alpha_keys[*lower].alpha;
    } else {
      const auto local_t = (t - alpha_keys[*lower].time) / (alpha_keys[*upper].time - alpha_keys[*lower].time);

      alpha = math::mix(alpha_keys[*lower].alpha, alpha_keys[*upper].alpha, local_t);
    }
  }

  return math::color{rgb.r(), rgb.g(), rgb.b(), alpha};
}

} // namespace sbx::assets
