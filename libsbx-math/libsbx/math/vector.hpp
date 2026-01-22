// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_VECTOR_HPP_
#define LIBSBX_MATH_VECTOR_HPP_

#include <array>
#include <cmath>
#include <ranges>

#include <libsbx/utility/make_array.hpp>
#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/zip.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/traits.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/algorithm.hpp>

namespace sbx::math {

template<std::size_t Size, scalar Type>
requires (Size > 1u)
class basic_vector {

  template<std::size_t S, scalar T>
  requires (S > 1u)
  friend class basic_vector;

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = std::size_t;
  using length_type = std::float_t;

  template<scalar Other = value_type>
  constexpr basic_vector(Other value = Other{0}) noexcept
  : _components{utility::make_array<value_type, Size>(value)} { }

  template<scalar Other = value_type>
  constexpr basic_vector(const basic_vector<Size, Other>& other) noexcept
  : _components{utility::make_array<value_type, Size>(other._components)} { }

  constexpr basic_vector(const basic_vector& other) noexcept = default;

  constexpr basic_vector(basic_vector&& other) noexcept = default;

  auto operator=(const basic_vector& other) noexcept -> basic_vector& = default;

  auto operator=(basic_vector&& other) noexcept -> basic_vector& = default;

  static constexpr auto min(const basic_vector& vector) noexcept -> value_type {
    auto result = vector[0];

    for (auto i : std::views::iota(1u, Size)) {
      result = std::min(result, vector[i]);
    }

    return result;
  }

  template<scalar Lhs = value_type, scalar Rhs = value_type>
  static constexpr auto min(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector {
    auto result = lhs;

    for (auto i : std::views::iota(0u, Size)) {
      result[i] = std::min(lhs[i], rhs[i]);
    }

    return result;
  }

  static constexpr auto max(const basic_vector& vector) noexcept -> value_type {
    auto result = vector[0];

    for (auto i : std::views::iota(1u, Size)) {
      result = std::max(result, vector[i]);
    }

    return result;
  }

  template<scalar Lhs = value_type, scalar Rhs = value_type>
  static constexpr auto max(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector {
    auto result = lhs;

    for (auto i : std::views::iota(0u, Size)) {
      result[i] = std::max(lhs[i], rhs[i]);
    }

    return result;
  }

  template<scalar Lhs = value_type, scalar Rhs = value_type>
  static constexpr auto abs(const basic_vector<Size, Lhs>& vector) noexcept -> basic_vector {
    auto result = vector;

    for (auto i : std::views::iota(0u, Size)) {
      result[i] = std::abs(vector[i]);
    }

    return result;
  }

  template<size_type Axis, scalar Other = value_type>
  requires (Axis < Size)
  [[nodiscard]] static constexpr auto splat(const basic_vector<Size, Other>& vector) noexcept -> basic_vector<Size, Other> {
    auto result = basic_vector<Size, Other>{};

    for (auto i : std::views::iota(0u, Size)) {
      result[i] = vector[Axis];
    }

    return result;
  }

  [[nodiscard]] static constexpr auto lerp(const basic_vector& x, const basic_vector& y, const value_type a) noexcept -> basic_vector {
    auto result = basic_vector{};

    for (auto i : std::views::iota(0u, Size)) {
      result[i] = math::mix(x[i], y[i], a);
    }

    return result;
  } 

  constexpr auto data() noexcept -> value_type* {
    return _components.data();
  }

  constexpr auto data() const noexcept -> const value_type* {
    return _components.data();
  }

  [[nodiscard]] constexpr auto operator[](size_type index) noexcept -> reference {
    return _components[index];
  }

  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> const_reference {
    return _components[index];
  }

  template<scalar Other>
  constexpr auto operator+=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
    for (auto i : std::views::iota(0u, Size)) {
      _components[i] += static_cast<value_type>(other[i]);
    }

    return *this;
  }

  template<scalar Other>
  constexpr auto operator-=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
    for (auto i : std::views::iota(0u, Size)) {
      _components[i] -= static_cast<value_type>(other[i]);
    }

    return *this;
  }

  template<scalar Other>
  constexpr auto operator*=(Other scalar) noexcept -> basic_vector& {
    for (auto i : std::views::iota(0u, Size)) {
      _components[i] *= static_cast<value_type>(scalar);
    }

    return *this;
  }

  template<scalar Other>
  constexpr auto operator*=(const basic_vector<Size, Other>& other) noexcept -> basic_vector& {
    for (auto i : std::views::iota(0u, Size)) {
      _components[i] *= static_cast<value_type>(other[i]);
    }

    return *this;
  }

  template<scalar Other>
  constexpr auto operator/=(Other scalar) noexcept -> basic_vector& {
    utility::assert_that(!comparision_traits<Other>::equal(scalar, static_cast<Other>(0)), "Division by zero");

    for (auto i : std::views::iota(0u, Size)) {
      _components[i] /= static_cast<value_type>(scalar);
    }

    return *this;
  }

  [[nodiscard]] constexpr auto length_squared() const noexcept -> length_type {
    auto result = static_cast<length_type>(0);

    for (auto i : std::views::iota(0u, Size)) {
      result += static_cast<length_type>(_components[i] * _components[i]);
    }

    return result;
  }

  [[nodiscard]] constexpr auto length() const noexcept -> length_type {
    return std::sqrt(length_squared());
  }

  constexpr auto normalize() noexcept -> basic_vector& {
    const auto length_squared = this->length_squared();

    if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
      *this /= std::sqrt(length_squared);
    }

    return *this;
  }

protected:

  template<std::convertible_to<value_type>... Args>
  requires (sizeof...(Args) == Size)
  constexpr basic_vector(Args&&... args) noexcept
  : _components{utility::make_array<value_type, Size>(std::forward<Args>(args)...)} { }

  constexpr basic_vector(std::array<value_type, Size>&& components) noexcept
  : _components{std::move(components)} { }

  template<scalar Other>
  [[nodiscard]] static constexpr auto fill(Other value) noexcept -> basic_vector {
    return basic_vector{value};
  }

  template<std::size_t Index, scalar Other>
  [[nodiscard]] static constexpr auto axis(Other value) noexcept -> basic_vector {
    return basic_vector{utility::make_array<value_type, Size, Index>(value)};
  }

private:

  std::array<Type, Size> _components;

}; // class basic_vector

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

template<std::size_t Size, scalar Type>
struct concrete_vector;

template<std::size_t Size, scalar Type>
using concrete_vector_t = typename concrete_vector<Size, Type>::type;

} // namespace sbx::math

#endif // LIBSBX_MATH_VECTOR_HPP_
