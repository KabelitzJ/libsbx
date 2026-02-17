// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_VOLUME_HPP_
#define LIBSBX_MATH_VOLUME_HPP_

#include <ranges>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

namespace sbx::math {

template<scalar Type>
class basic_volume {

public:

  using value_type = Type;
  using vector_type = basic_vector3<value_type>;

  basic_volume() noexcept = default;
  
  basic_volume(const vector_type& min, const vector_type& max) noexcept
  : _min{min}, 
    _max{max} { }

  static auto transformed(const basic_volume& volume, const math::matrix4x4& matrix) -> basic_volume {
    auto min = math::vector3{std::numeric_limits<std::float_t>::max()};
    auto max = math::vector3{std::numeric_limits<std::float_t>::lowest()};

    for (const auto& corner : volume.corners()) {
      const auto transformed = math::vector3{matrix * math::vector4{corner, 1.0f}};
      min = math::vector3::min(min, transformed);
      max = math::vector3::max(max, transformed);
    }

    return basic_volume{min, max};
  }

  auto min() const noexcept -> const vector_type& {
    return _min;
  }

  auto max() const noexcept -> const vector_type& {
    return _max;
  }

  auto center() const noexcept -> vector_type {
    return (_min + _max) / 2.0f;
  }

  auto corners() const noexcept -> std::array<math::vector3, 8u> {
    return std::array<math::vector3, 8u>{
      math::vector3{_min.x(), _min.y(), _min.z()},
      math::vector3{_min.x(), _min.y(), _max.z()},
      math::vector3{_min.x(), _max.y(), _min.z()},
      math::vector3{_min.x(), _max.y(), _max.z()},
      math::vector3{_max.x(), _min.y(), _min.z()},
      math::vector3{_max.x(), _min.y(), _max.z()},
      math::vector3{_max.x(), _max.y(), _min.z()},
      math::vector3{_max.x(), _max.y(), _max.z()}
    };
  }

  auto contains(const vector_type& point) const noexcept -> bool {
    return point.x() >= _min.x() && point.x() <= _max.x() && point.y() >= _min.y() && point.y() <= _max.y() && point.z() >= _min.z() && point.z() <= _max.z();
  }

  auto contains(const basic_volume& other) const noexcept -> bool {
    return _min.x() <= other.min().x() && _min.y() <= other.min().y() && _min.z() <= other.min().z() && _max.x() >= other.max().x() && _max.y() >= other.max().y() && _max.z() >= other.max().z();
  }

  auto intersects(const basic_volume& other) const noexcept -> bool {
    return _min.x() <= other.max().x() && _max.x() >= other.min().x() && _min.y() <= other.max().y() && _max.y() >= other.min().y() && _min.z() <= other.max().z() && _max.z() >= other.min().z();
  }

  auto extend() const noexcept -> math::vector3 {
    return _max - _min;
  }

  auto diagonal_length() const noexcept -> value_type {
    return extend().length();
  }

  auto is_empty() const noexcept -> bool {
    return _min.x() >= _max.x() || _min.y() >= _max.y() || _min.z() >= _max.z();
  }

  auto include(const basic_volume& other) noexcept -> void {
    _min = vector_type::min(_min, other.min());
    _max = vector_type::max(_max, other.max());
  }

  static auto merge(const basic_volume& a, const basic_volume& b) -> basic_volume {
    return basic_volume{
      vector_type::min(a.min(), b.min()),
      vector_type::max(a.max(), b.max())
    };
  }

  static auto are_overlapping(const basic_volume& a, const basic_volume& b) -> bool {
    return (a.min().x() <= b.max().x() && a.max().x() >= b.min().x()) && (a.min().y() <= b.max().y() && a.max().y() >= b.min().y()) && (a.min().z() <= b.max().z() && a.max().z() >= b.min().z());
  }

  template<std::ranges::input_range Range, typename Projection = std::identity>
  requires (std::convertible_to<std::invoke_result_t<Projection, std::ranges::range_reference_t<Range>>, vector_type>)
  static auto construct(Range&& range, Projection projection = {}) -> basic_volume {
    auto iterator = std::ranges::begin(range);
    const auto end = std::ranges::end(range);

    if (iterator == end) {
      return basic_volume{};
    }

    auto first_point = std::invoke(projection, *iterator);

    auto min = first_point;
    auto max = first_point;

    ++iterator;

    for (; iterator != end; ++iterator) {
      const auto point = std::invoke(projection, *iterator);

      min = vector_type::min(min, point);
      max = vector_type::max(max, point);
    }

    return basic_volume{min, max};
  }

private:

  vector_type _min;
  vector_type _max;

}; // class basic_volume

using volumef = basic_volume<std::float_t>;

using volume = volumef;

} // namespace sbx::math

#endif // LIBSBX_MATH_VOLUME_HPP_
