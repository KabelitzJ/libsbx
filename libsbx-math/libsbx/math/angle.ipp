// SPDX-License-Identifier: MIT
#include <libsbx/math/angle.hpp>

namespace sbx::math {

template<floating_point Type>
template<scalar Other>
requires (std::is_convertible_v<Other, Type>)
constexpr basic_degree<Type>::basic_degree(const Other value) noexcept
: _value{static_cast<Type>(value)} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr basic_degree<Type>::basic_degree(const basic_degree<Other>& other) noexcept
: _value{static_cast<Type>(other._value)} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_degree<Type>::operator=(const basic_degree<Other>& other) noexcept -> basic_degree<Type>& {
  _value = static_cast<Type>(other._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_degree<Type>::operator+=(const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>& {
  _value += static_cast<Type>(rhs._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_degree<Type>::operator-=(const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>& {
  _value -= static_cast<Type>(rhs._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_degree<Type>::operator*=(const Other rhs) noexcept -> basic_degree<Type>& {
  _value *= static_cast<Type>(rhs);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_degree<Type>::operator/=(const Other rhs) noexcept -> basic_degree<Type>& {
  _value /= static_cast<Type>(rhs);

  return *this;
}

template<floating_point Type>
constexpr auto basic_degree<Type>::value() const noexcept -> value_type {
  return _value;
}

template<floating_point Type>
constexpr basic_degree<Type>::operator value_type() const noexcept {
  return _value;
}

template<floating_point Type>
constexpr auto basic_degree<Type>::data() noexcept -> value_type* {
  return &_value;
}

template<floating_point Type>
constexpr auto operator==(const basic_degree<Type>& lhs, const basic_degree<Type>& rhs) noexcept -> bool {
  return static_cast<basic_degree<Type>::value_type>(lhs) == static_cast<basic_degree<Type>::value_type>(rhs);
}

template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_degree<Type>& lhs, const basic_degree<Other>& rhs) noexcept -> std::partial_ordering {
  return static_cast<basic_degree<Type>::value_type>(lhs) <=> static_cast<basic_degree<Type>::value_type>(rhs);
}

template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator+(basic_degree<Type> lhs, const basic_degree<Other>& rhs) noexcept -> basic_degree<Type> {
  lhs += rhs;

  return lhs;
}

template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator-(basic_degree<Type> lhs, const basic_degree<Other>& rhs) noexcept -> basic_degree<Type> {
  lhs -= rhs;

  return lhs;
}

template<floating_point Type, std::convertible_to<Type> Other>
constexpr auto operator*(basic_degree<Type> lhs, const Other rhs) noexcept -> basic_degree<Type> {
  lhs *= static_cast<Type>(rhs);

  return lhs;
}

template<floating_point Type, std::convertible_to<Type> Other>
constexpr auto operator/(basic_degree<Type> lhs, const Other rhs) noexcept -> basic_degree<Type> {
  lhs /= static_cast<Type>(rhs);

  return lhs;
}

template<floating_point Type>
constexpr auto clamp(const basic_degree<Type>& value, const basic_degree<Type>& min, const basic_degree<Type>& max) -> const basic_degree<Type>& {
  if (value < min) {
    return min;
  }

  if (value > max) {
    return max;
  }

  return value;
}

template<floating_point Type>
inline constexpr auto comparision_traits<basic_degree<Type>>::equal(const basic_degree<Type>& lhs, const basic_degree<Type>& rhs) noexcept -> bool {
  return base_trait::equal(lhs.value(), rhs.value());
}

template<floating_point Type>
inline constexpr auto mix(const basic_degree<Type> x, const basic_degree<Type> y, const Type a) -> basic_degree<Type> {
  return basic_degree<Type>{x.value() * (static_cast<Type>(1) - a) + y.value() * a};
}

template<floating_point Type>
template<scalar Other>
requires (std::is_convertible_v<Other, Type>)
constexpr basic_radian<Type>::basic_radian(const Other value) noexcept
: _value{static_cast<Type>(value)} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr basic_radian<Type>::basic_radian(const basic_radian<Other>& other) noexcept
: _value{static_cast<Type>(other._value)} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_radian<Type>::operator=(const basic_radian<Other>& other) noexcept -> basic_radian<Type>& {
  _value = static_cast<Type>(other._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_radian<Type>::operator+=(const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>& {
  _value += static_cast<Type>(rhs._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_radian<Type>::operator-=(const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>& {
  _value -= static_cast<Type>(rhs._value);

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_radian<Type>::operator*=(const Other rhs) noexcept -> basic_radian<Type>& {
  _value *= static_cast<Type>(rhs);

  return *this;
}

template<floating_point Type>
constexpr auto basic_radian<Type>::value() const noexcept -> value_type {
  return _value;
}

template<floating_point Type>
constexpr basic_radian<Type>::operator value_type() const noexcept {
  return _value;
}

template<floating_point Type>
constexpr auto basic_radian<Type>::data() noexcept -> value_type* {
  return &_value;
}

template<floating_point Type>
constexpr auto operator==(const basic_radian<Type>& lhs, const basic_radian<Type>& rhs) noexcept -> bool {
  return static_cast<basic_radian<Type>::value_type>(lhs) == static_cast<basic_radian<Type>::value_type>(rhs);
}

template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_radian<Type>& lhs, const basic_radian<Other>& rhs) noexcept -> std::partial_ordering {
  return static_cast<basic_radian<Type>::value_type>(lhs) <=> static_cast<basic_radian<Type>::value_type>(rhs);
}

template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator+(basic_radian<Type> lhs, const basic_radian<Other>& rhs) noexcept -> basic_radian<Type> {
  lhs += rhs;

  return lhs;
}

template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator-(basic_radian<Type> lhs, const basic_radian<Other>& rhs) noexcept -> basic_radian<Type> {
  lhs -= rhs;

  return lhs;
}

template<floating_point Type, std::convertible_to<Type> Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator*(basic_radian<Type> lhs, const Other rhs) noexcept -> basic_radian<Type> {
  lhs *= static_cast<Type>(rhs);

  return lhs;
}

template<floating_point Type>
constexpr auto clamp(const basic_radian<Type>& value, const basic_radian<Type>& min, const basic_radian<Type>& max) -> const basic_radian<Type>& {
  if (value < min) {
    return min;
  }

  if (value > max) {
    return max;
  }

  return value;
}

template<floating_point Type>
constexpr basic_angle<Type>::basic_angle(const basic_degree<value_type>& degree) noexcept
: _radian{degree.value() * std::numbers::pi_v<value_type> / static_cast<value_type>(180)} { }

template<floating_point Type>
constexpr basic_angle<Type>::basic_angle(const basic_radian<value_type>& radian) noexcept
: _radian{radian} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr basic_angle<Type>::basic_angle(const basic_angle<Other>& other) noexcept
: _radian{other._radian} { }

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator+=(const basic_angle<Other>& other) noexcept -> basic_angle<Type>& {
  _radian += basic_radian<Type>{other._radian};

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator+=(const basic_degree<Other>& other) noexcept -> basic_angle<Type>& {

  return (*this += basic_angle<Other>{other});
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator+=(const basic_radian<Other>& other) noexcept -> basic_angle<Type>& {

  return (*this += basic_angle<Other>{other});
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator-=(const basic_angle<Other>& other) noexcept -> basic_angle<Type>& {
  _radian -= basic_radian<Type>{other._radian};

  return *this;
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator-=(const basic_degree<Other>& other) noexcept -> basic_angle<Type>& {
  return (*this -= basic_angle<Other>{other});
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator-=(const basic_radian<Other>& other) noexcept -> basic_angle<Type>& {
  return (*this -= basic_angle<Other>{other});
}

template<floating_point Type>
template<floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto basic_angle<Type>::operator*=(const Other other) noexcept -> basic_angle<Type>& {
  _radian = basic_radian<Type>{_radian.value() * static_cast<Type>(other)};

  return *this;
}

template<floating_point Type>
constexpr auto basic_angle<Type>::to_degrees() const noexcept -> basic_degree<value_type> {
  return basic_degree<value_type>{_radian.value() * static_cast<value_type>(180) / std::numbers::pi_v<value_type>};
}

template<floating_point Type>
constexpr auto basic_angle<Type>::to_radians() const noexcept -> basic_radian<value_type> {
  return _radian;
}

template<floating_point Type>
constexpr auto operator==(const basic_angle<Type>& lhs, const basic_angle<Type>& rhs) noexcept -> bool {
  return lhs.to_radians() == rhs.to_radians();
}

template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_angle<Type>& lhs, const basic_angle<Other>& rhs) noexcept -> std::partial_ordering {
  return lhs.to_radians() <=> rhs.to_radians();
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_angle<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs += rhs;

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_degree<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs += basic_angle<LhsType>(rhs);

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_radian<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs += basic_angle<LhsType>(rhs);

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_angle<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs -= rhs;

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_degree<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs -= basic_angle<LhsType>(rhs);

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_radian<RhsType>& rhs) noexcept -> basic_angle<LhsType> {
  lhs -= basic_angle<LhsType>(rhs);

  return lhs;
}

template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator*(basic_angle<LhsType> lhs, const RhsType rhs) noexcept -> basic_angle<LhsType> {
  lhs *= rhs;

  return lhs;
}

template<floating_point Type>
constexpr auto clamp(const basic_angle<Type>& value, const basic_angle<Type>& min, const basic_angle<Type>& max) -> const basic_angle<Type>& {
  if (value < min) {
    return min;
  }

  if (value > max) {
    return max;
  }

  return value;
}

template<floating_point Type>
constexpr auto to_degrees(const basic_radian<Type>& radian) noexcept -> basic_degree<Type> {
  return basic_angle<Type>{radian}.to_degrees();
}

template<floating_point Type>
constexpr auto to_radians(const basic_degree<Type>& degree) noexcept -> basic_radian<Type> {
  return basic_angle<Type>{degree}.to_radians();
}

template<floating_point Type>
constexpr auto sin(const basic_angle<Type>& angle) noexcept -> Type {
  return std::sin(angle.to_radians().value());
}

template<floating_point Type>
constexpr auto sin(const basic_degree<Type>& degree) noexcept -> Type {
  return sin(basic_angle<Type>{degree});
}

template<floating_point Type>
constexpr auto sin(const basic_radian<Type>& radian) noexcept -> Type {
  return sin(basic_angle<Type>{radian});
}

template<floating_point Type>
constexpr auto cos(const basic_angle<Type>& angle) noexcept -> Type {
  return std::cos(angle.to_radians().value());
}

template<floating_point Type>
constexpr auto cos(const basic_degree<Type>& degree) noexcept -> Type {
  return cos(basic_angle<Type>{degree});
}

template<floating_point Type>
constexpr auto cos(const basic_radian<Type>& radian) noexcept -> Type {
  return cos(basic_angle<Type>{radian});
}

template<floating_point Type>
constexpr auto tan(const basic_angle<Type>& angle) noexcept -> Type {
  return std::tan(angle.to_radians().value());
}

namespace literals {

constexpr auto operator""_deg(long double value) noexcept -> degree {
  return degree{static_cast<std::float_t>(value)};
}

constexpr auto operator""_deg(unsigned long long value) noexcept -> degree {
  return degree{static_cast<std::float_t>(value)};
}

constexpr auto operator""_rad(long double value) noexcept -> radian {
  return radian{static_cast<std::float_t>(value)};
}

constexpr auto operator""_rad(unsigned long long value) noexcept -> radian {
  return radian{static_cast<std::float_t>(value)};
}

} // namespace literals

} // namespace sbx::math
