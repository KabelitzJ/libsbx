// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_RANDOM_HPP_
#define LIBSBX_MATH_RANDOM_HPP_

#include <random>
#include <ranges>
#include <concepts>
#include <limits>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/concepts.hpp>

namespace sbx::math {

class random {

public:

  using generator_type = std::mt19937;

  random() = delete;

  template<numeric Type>
  static auto next(Type min = std::numeric_limits<Type>::min(), Type max = std::numeric_limits<Type>::max()) -> Type {
    using distribution_type = std::conditional_t<std::floating_point<Type>, std::uniform_real_distribution<Type>, std::uniform_int_distribution<Type>>;

    auto distribution = distribution_type{min, max};

    return distribution(_generator());
  }

  template<integral Seed>
  static auto seed(const Seed seed) -> void {
    _generator().seed(static_cast<generator_type::result_type>(seed));
  }

private:

  static auto _generator() -> generator_type& {
    static auto device = std::random_device{};
    static auto generator = generator_type{device()};

    return generator;
  }

}; // struct random

template<std::ranges::random_access_range Range>
auto shuffle_range(Range& range) -> void {
  for (auto i = std::ranges::size(range); i > 1u; --i) {
    const auto j = sbx::math::random::next<std::size_t>(0u, i - 1u);

    std::swap(range[i - 1u], range[j]);
  }
}

template<std::ranges::sized_range Range>
auto random_element(const Range& range) -> std::ranges::range_value_t<Range> {
  const auto size = static_cast<std::ranges::range_difference_t<Range>>(std::ranges::size(range));
  const auto index = random::next<std::ranges::range_difference_t<Range>>(0, size - 1);

  return *std::next(std::begin(range), index);
}

template<scalar Type>
auto random_point_in_circle(const basic_vector2<Type>& center, const Type radius) -> basic_vector2<Type> {
  const auto r = radius * std::sqrt(random::next<Type>(Type{0}, Type{1}));
  const auto theta = random::next<Type>(Type{0}, Type{2} * std::numbers::pi_v<Type>);

  return center + basic_vector2<Type>{r * std::cos(theta), r * std::sin(theta)};
}

template<scalar Type>
auto random_point_on_circle(const basic_vector2<Type>& center, const Type radius) -> basic_vector2<Type> {
  const auto theta = random::next<Type>(Type{0}, Type{2} * std::numbers::pi_v<Type>);

  return center + basic_vector2<Type>{radius * std::cos(theta), radius * std::sin(theta)};
}

template<scalar Type>
auto random_point_in_sphere(const basic_vector3<Type>& center, const Type radius) -> basic_vector3<Type> {
  const auto r = radius * std::cbrt(random::next<Type>(Type{0}, Type{1}));
  const auto theta = random::next<Type>(Type{0}, Type{2} * std::numbers::pi_v<Type>);
  const auto phi = random::next<Type>(Type{0}, std::numbers::pi_v<Type>);

  const auto x = r * std::sin(phi) * std::cos(theta);
  const auto y = r * std::sin(phi) * std::sin(theta);
  const auto z = r * std::cos(phi);

  return center + basic_vector3<Type>{x, y, z};
}

auto random_color(const std::float_t alpha = 1.0f) -> color;

} // namespace sbx::math

#endif // LIBSBX_MATH_RANDOM_HPP_
