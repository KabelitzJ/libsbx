// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_MATERIAL_HPP_
#define LIBSBX_ASSETS_MATERIAL_HPP_

#include <cstdint>
#include <limits>
#include <string>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

class material final {

  friend class assets_module;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  struct create_info {
    std::string name{"material"};
    math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    math::vector3 emissive_factor{0.0f, 0.0f, 0.0f};
    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    texture_handle albedo{};
    texture_handle normal{};
    texture_handle metallic_roughness{};
    texture_handle occlusion{};
    texture_handle emissive{};
  }; // struct create_info

  material() = default;

  explicit material(const create_info& create_info)
  : _base_color_factor{create_info.base_color_factor},
    _emissive_factor{create_info.emissive_factor},
    _metallic_factor{create_info.metallic_factor},
    _roughness_factor{create_info.roughness_factor},
    _albedo{create_info.albedo},
    _normal{create_info.normal},
    _metallic_roughness{create_info.metallic_roughness},
    _occlusion{create_info.occlusion},
    _emissive{create_info.emissive},
    _name{create_info.name} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool { 
    return _index != invalid_index; 
  }

  [[nodiscard]] auto index() const noexcept -> std::uint32_t { 
    return _index; 
  }

  [[nodiscard]] auto base_color_factor() const noexcept -> const math::color& { 
    return _base_color_factor; 
  }

  [[nodiscard]] auto emissive_factor() const noexcept -> const math::vector3& { 
    return _emissive_factor; 
  }

  [[nodiscard]] auto metallic_factor() const noexcept -> float { 
    return _metallic_factor; 
  }

  [[nodiscard]] auto roughness_factor() const noexcept -> float { 
    return _roughness_factor; 
  }


  [[nodiscard]] auto albedo() const noexcept -> const texture_handle& { 
    return _albedo; 
 }

  [[nodiscard]] auto normal() const noexcept -> const texture_handle& { 
    return _normal; 
  }

  [[nodiscard]] auto metallic_roughness() const noexcept -> const texture_handle& { 
    return _metallic_roughness; 
  }

  [[nodiscard]] auto occlusion() const noexcept -> const texture_handle& { 
    return _occlusion; 
  }

  [[nodiscard]] auto emissive() const noexcept -> const texture_handle& { 
    return _emissive; 
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& { 
    return _name; 
  }

private:

  math::color _base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
  math::vector3 _emissive_factor{0.0f, 0.0f, 0.0f};
  float _metallic_factor{1.0f};
  float _roughness_factor{1.0f};
  texture_handle _albedo{};
  texture_handle _normal{};
  texture_handle _metallic_roughness{};
  texture_handle _occlusion{};
  texture_handle _emissive{};
  std::uint32_t _index{invalid_index};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"material"};

}; // class material

using material_handle = asset_handle<material>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_MATERIAL_HPP_
