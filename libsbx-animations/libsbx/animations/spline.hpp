// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SPLINE_HPP_
#define LIBSBX_ANIMATIONS_SPLINE_HPP_

#include <vector>
#include <algorithm>

#include <libsbx/utility/assert.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::animations {

template<typename Type>
class spline {

public:

  auto add(const std::float_t timestamp, const Type& value) -> void {
    _timestamps.push_back(timestamp);
    _values.push_back(value);
  }

  auto sample(const std::float_t time) const -> Type {
    auto entry = std::lower_bound(_timestamps.begin(), _timestamps.end(), time);

    if (entry == _timestamps.begin()) {
      return _values.front();
    }

    if (entry == _timestamps.end()) {
      return _values.back();
    }

    const auto i = entry - _timestamps.begin();

    const auto dt = _timestamps[i] - _timestamps[i - 1];
    const auto t = (dt > math::epsilonf) ? (time - _timestamps[i - 1]) / dt : 0.0f;

    if constexpr (std::is_same_v<Type, math::quaternion>) {
      return math::quaternion::slerp(_values[i - 1], _values[i], t);
    } else {
      return math::vector3::lerp(_values[i - 1], _values[i], t);
    }
  }

  auto size() const noexcept -> std::size_t {
    return _timestamps.size();
  }

private:

  std::vector<std::float_t> _timestamps;
  std::vector<Type> _values;

}; // class spline

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_SPLINE_HPP_
