// SPDX-License-Identifier: MIT
#include <libsbx/math/vector.hpp>

namespace sbx::math {

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr basic_vector<Size, Type>::basic_vector(Other value) noexcept
: _components{utility::make_array<value_type, Size>(value)} { }

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr basic_vector<Size, Type>::basic_vector(const basic_vector<Size, Other>& other) noexcept
: _components{utility::make_array<value_type, Size>(other._components)} { }

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr auto basic_vector<Size, Type>::min(const basic_vector& vector) noexcept -> value_type {
  auto result = vector[0];

  for (auto i : std::views::iota(1u, Size)) {
    result = std::min(result, vector[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Lhs, scalar Rhs>
constexpr auto basic_vector<Size, Type>::min(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector {
  auto result = lhs;

  for (auto i : std::views::iota(0u, Size)) {
    result[i] = std::min(lhs[i], rhs[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr auto basic_vector<Size, Type>::max(const basic_vector& vector) noexcept -> value_type {
  auto result = vector[0];

  for (auto i : std::views::iota(1u, Size)) {
    result = std::max(result, vector[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Lhs, scalar Rhs>
constexpr auto basic_vector<Size, Type>::max(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector {
  auto result = lhs;

  for (auto i : std::views::iota(0u, Size)) {
    result[i] = std::max(lhs[i], rhs[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Lhs>
constexpr auto basic_vector<Size, Type>::abs(const basic_vector<Size, Lhs>& vector) noexcept -> basic_vector {
  auto result = vector;

  for (auto i : std::views::iota(0u, Size)) {
    result[i] = std::abs(vector[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<typename basic_vector<Size, Type>::size_type Axis, scalar Other>
requires (Axis < Size)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::splat(const basic_vector<Size, Other>& vector) noexcept -> basic_vector<Size, Other> {
  auto result = basic_vector<Size, Other>{};

  for (auto i : std::views::iota(0u, Size)) {
    result[i] = vector[Axis];
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::lerp(const basic_vector& x, const basic_vector& y, const value_type a) noexcept -> basic_vector {
  auto result = basic_vector{};

  for (auto i : std::views::iota(0u, Size)) {
    result[i] = math::mix(x[i], y[i], a);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr auto basic_vector<Size, Type>::data() noexcept -> value_type* {
  return _components.data();
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr auto basic_vector<Size, Type>::data() const noexcept -> const value_type* {
  return _components.data();
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::operator[](size_type index) noexcept -> reference {
  return _components[index];
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::operator[](size_type index) const noexcept -> const_reference {
  return _components[index];
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr auto basic_vector<Size, Type>::operator+=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
  for (auto i : std::views::iota(0u, Size)) {
    _components[i] += static_cast<value_type>(other[i]);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr auto basic_vector<Size, Type>::operator-=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
  for (auto i : std::views::iota(0u, Size)) {
    _components[i] -= static_cast<value_type>(other[i]);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr auto basic_vector<Size, Type>::operator*=(Other scalar) noexcept -> basic_vector& {
  for (auto i : std::views::iota(0u, Size)) {
    _components[i] *= static_cast<value_type>(scalar);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr auto basic_vector<Size, Type>::operator*=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
  for (auto i : std::views::iota(0u, Size)) {
    _components[i] *= static_cast<value_type>(other[i]);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
constexpr auto basic_vector<Size, Type>::operator/=(Other scalar) noexcept -> basic_vector& {
  utility::assert_that(!comparision_traits<Other>::equal(scalar, static_cast<Other>(0)), "Division by zero");

  for (auto i : std::views::iota(0u, Size)) {
    _components[i] /= static_cast<value_type>(scalar);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::length_squared() const noexcept -> length_type {
  auto result = static_cast<length_type>(0);

  for (auto i : std::views::iota(0u, Size)) {
    result += static_cast<length_type>(_components[i] * _components[i]);
  }

  return result;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
[[nodiscard]] constexpr auto basic_vector<Size, Type>::length() const noexcept -> length_type {
  return std::sqrt(length_squared());
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr auto basic_vector<Size, Type>::normalize() noexcept -> basic_vector& {
  const auto length_squared = this->length_squared();

  if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
    *this /= std::sqrt(length_squared);
  }

  return *this;
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<std::convertible_to<typename basic_vector<Size, Type>::value_type>... Args>
requires (sizeof...(Args) == Size)
constexpr basic_vector<Size, Type>::basic_vector(Args&&... args) noexcept
: _components{utility::make_array<value_type, Size>(std::forward<Args>(args)...)} { }

template<std::size_t Size, scalar Type>
requires (Size > 1u)
constexpr basic_vector<Size, Type>::basic_vector(std::array<value_type, Size>&& components) noexcept
: _components{std::move(components)} { }

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<scalar Other>
[[nodiscard]] constexpr auto basic_vector<Size, Type>::fill(Other value) noexcept -> basic_vector {
  return basic_vector{value};
}

template<std::size_t Size, scalar Type>
requires (Size > 1u)
template<std::size_t Index, scalar Other>
[[nodiscard]] constexpr auto basic_vector<Size, Type>::axis(Other value) noexcept -> basic_vector {
  return basic_vector{utility::make_array<value_type, Size, Index>(value)};
}

template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator==(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> bool {
  for (auto i : std::views::iota(0u, Size)) {
    if (!comparision_traits<Lhs>::equal(lhs[i], rhs[i])) {
      return false;
    }
  }

  return true;
}

template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_vector<Size, Lhs> lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector<Size, Lhs> {
  return lhs += rhs;
}

template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_vector<Size, Lhs> lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector<Size, Lhs> {
  return lhs -= rhs;
}

template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector<Size, Lhs> lhs, Rhs scalar) noexcept -> basic_vector<Size, Lhs> {
  return lhs *= scalar;
}

template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator/(basic_vector<Size, Lhs> lhs, Rhs scalar) noexcept -> basic_vector<Size, Lhs> {
  return lhs /= scalar;
}

} // namespace sbx::math
