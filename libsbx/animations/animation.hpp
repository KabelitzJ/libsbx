// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_ANIMATION_HPP_
#define LIBSBX_ANIMATIONS_ANIMATION_HPP_

#include <string>
#include <vector>
#include <filesystem>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset.hpp>

#include <libsbx/animations/spline.hpp>

namespace sbx::animations {

class animation : public assets::asset {

public:

  struct bone_track {
    spline<math::vector3> position_spline;
    spline<math::quaternion> rotation_spline;
    spline<math::vector3> scale_spline;
  }; // struct bone_track

  animation(const std::filesystem::path& path, const std::string& name);

  ~animation() override = default;

  /**
   * @brief Returns the names of every animation clip contained in @p path, in file order. Empty if the file has none or cannot be read.
   *
   * Used by the importer to enumerate the `animation:<name>` sub-assets a source exposes.
   */
  static auto clip_names(const std::filesystem::path& path) -> std::vector<std::string>;

  auto type() const -> assets::asset_type override {
    return assets::asset_type::animation;
  }

  auto track_map() const noexcept -> const std::unordered_map<utility::hashed_string, bone_track>&;

  auto duration() const noexcept -> std::float_t;

private:

  std::string _name;
  std::float_t _duration = 0.0f;
  std::float_t _ticks_per_second = 25.0f;

  std::unordered_map<utility::hashed_string, bone_track> _track_map;

}; // class animation

struct animation_handle : public math::uuid {

  constexpr animation_handle()
  : math::uuid{math::uuid::nil()}  { }

  constexpr explicit animation_handle(const math::uuid& id)
  : math::uuid{id} { }

  constexpr auto is_valid() const noexcept -> bool {
    return (*this) != math::uuid::nil();
  }

  constexpr auto operator==(const animation_handle& other) const noexcept -> bool = default;

}; // struct animation_handle

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_ANIMATION_HPP_
