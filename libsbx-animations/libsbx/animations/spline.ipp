// SPDX-License-Identifier: MIT
#include <libsbx/animations/spline.hpp>

namespace sbx::animations {

template<typename Type>
auto spline<Type>::add(const std::float_t timestamp, const Type& value) -> void {
  _timestamps.push_back(timestamp);
  _values.push_back(value);
}

template<typename Type>
auto spline<Type>::sample(const std::float_t time) const -> Type {
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

template<typename Type>
auto spline<Type>::size() const noexcept -> std::size_t {
  return _timestamps.size();
}

} // namespace sbx::animations
