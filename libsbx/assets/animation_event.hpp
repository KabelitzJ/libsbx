// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ANIMATION_EVENT_HPP_
#define LIBSBX_ASSETS_ANIMATION_EVENT_HPP_

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sbx::assets {

/** @brief A named moment in an animation clip (a footstep, a muzzle flash), delivered to scripts as the clip plays past it. */
struct animation_event {
  std::float_t time{0.0f}; // seconds into the clip
  std::string name{};
  std::float_t float_parameter{0.0f};
  std::int32_t int_parameter{0};
  std::string string_parameter{};

  // The least weight the clip must play at (its share of the blend, crossfade and layer) for the event to fire, so a clip barely blended in doesn't fire its footsteps.
  std::float_t min_weight{0.5f};

  auto operator==(const animation_event&) const -> bool = default;
}; // struct animation_event

/** @brief The events of one clip of a mesh, matched to the clip by name. */
struct animation_clip_events {
  std::string clip_name{};
  std::vector<animation_event> events{};

  auto operator==(const animation_clip_events&) const -> bool = default;
}; // struct animation_clip_events

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ANIMATION_EVENT_HPP_
