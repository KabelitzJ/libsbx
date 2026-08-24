// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
#define LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

enum class emitter_blend_mode : std::uint8_t {
  additive,
  alpha_blend
}; // enum class emitter_blend_mode

enum class emitter_shape : std::uint8_t {
  point,
  sphere,
  box
}; // enum class emitter_shape

struct particle_emitter {
  std::string name{"emitter"};
  emitter_blend_mode blend_mode{emitter_blend_mode::additive};
  std::float_t emission_rate{10.0f};
  std::uint32_t burst_count{0u};
  emitter_shape shape{emitter_shape::point};
  math::vector3 shape_extents{0.0f, 0.0f, 0.0f};
  math::vector3 velocity_min{-1.0f, 1.0f, -1.0f};
  math::vector3 velocity_max{1.0f, 2.0f, 1.0f};
  std::float_t lifetime_min{1.0f};
  std::float_t lifetime_max{2.0f};
  math::color start_color{1.0f, 1.0f, 1.0f, 1.0f};
  math::color end_color{1.0f, 1.0f, 1.0f, 0.0f};
  std::float_t size_min{0.1f};
  std::float_t size_max{0.2f};
  std::float_t gravity{0.0f};
  std::float_t drag{0.0f};
  texture_handle texture{};
}; // struct particle_emitter

class particle_effect final {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"particle_effect"};
    std::vector<particle_emitter> emitters{};
  }; // struct create_info

  particle_effect() = default;

  explicit particle_effect(const create_info& create_info)
  : _emitters{create_info.emitters},
    _name{create_info.name} { }

  [[nodiscard]] auto emitters() const noexcept -> const std::vector<particle_emitter>& {
    return _emitters;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  std::vector<particle_emitter> _emitters{};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"particle_effect"};

}; // class particle_effect

using particle_effect_handle = asset_handle<particle_effect>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_PARTICLE_EFFECT_HPP_
