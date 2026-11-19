// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ANIMATION_CLIP_HPP_
#define LIBSBX_ASSETS_ANIMATION_CLIP_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/animation_event.hpp>
#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

enum class animation_interpolation : std::uint8_t {
  linear = 0u,
  step = 1u,
  cubic_spline = 2u
}; // enum class animation_interpolation

template<typename Value>
struct animation_key {
  std::float_t time{0.0f};
  Value value{};
}; // struct animation_key

struct animation_joint_channel {
  std::uint32_t joint_index{0u};
  std::vector<animation_key<math::vector3>> translation_keys{};
  std::vector<animation_key<math::quaternion>> rotation_keys{};
  std::vector<animation_key<math::vector3>> scale_keys{};
  animation_interpolation translation_interpolation{animation_interpolation::linear};
  animation_interpolation rotation_interpolation{animation_interpolation::linear};
  animation_interpolation scale_interpolation{animation_interpolation::linear};
}; // struct animation_joint_channel

class animation_clip final : public loadable {

  friend class asset_residency;

public:

  animation_clip() = default;

  animation_clip(std::string name, std::float_t duration, std::vector<animation_joint_channel> channels)
  : _name{std::move(name)}, 
    _duration{duration}, 
    _channels{std::move(channels)} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_channels.empty();
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

  [[nodiscard]] auto duration() const noexcept -> std::float_t {
    return _duration;
  }

  [[nodiscard]] auto channels() const noexcept -> const std::vector<animation_joint_channel>& {
    return _channels;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  /** @brief The clip's events from its mesh's import settings, sorted by time. */
  [[nodiscard]] auto events() const noexcept -> const std::vector<animation_event>& {
    return _events;
  }

private:

  auto _finalize_content(std::string name, std::float_t duration, std::vector<animation_joint_channel> channels) -> void {
    _name = std::move(name);
    _duration = duration;
    _channels = std::move(channels);
    _bump_generation();
  }

  std::string _name{};
  std::float_t _duration{0.0f};
  std::vector<animation_joint_channel> _channels{};
  std::vector<animation_event> _events{};
  math::uuid _id{math::uuid::nil()};

}; // class animation_clip

using animation_clip_handle = asset_handle<animation_clip>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ANIMATION_CLIP_HPP_
