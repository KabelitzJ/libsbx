// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cstdint>

#include <gtest/gtest.h>

#include <libsbx/math/vector3.hpp>

using namespace sbx::math;

// -- Algebra: addition, scaling, dot and cross products -- these must hold for every scalar type.

template<typename Type>
class vector3_algebra_test : public ::testing::Test { };

using algebra_scalar_types = ::testing::Types<std::float_t, std::double_t, std::int32_t>;
TYPED_TEST_SUITE(vector3_algebra_test, algebra_scalar_types);

TYPED_TEST(vector3_algebra_test, addition_and_subtraction) {
  using vector3 = basic_vector3<TypeParam>;

  const auto lhs = vector3{TypeParam{1}, TypeParam{2}, TypeParam{3}};
  const auto rhs = vector3{TypeParam{4}, TypeParam{5}, TypeParam{6}};

  const auto sum = lhs + rhs;
  EXPECT_EQ(sum.x(), TypeParam{5});
  EXPECT_EQ(sum.y(), TypeParam{7});
  EXPECT_EQ(sum.z(), TypeParam{9});

  const auto difference = rhs - lhs;
  EXPECT_EQ(difference.x(), TypeParam{3});
  EXPECT_EQ(difference.y(), TypeParam{3});
  EXPECT_EQ(difference.z(), TypeParam{3});
}

TYPED_TEST(vector3_algebra_test, scalar_multiplication_and_division) {
  using vector3 = basic_vector3<TypeParam>;

  const auto vector = vector3{TypeParam{2}, TypeParam{4}, TypeParam{6}};

  const auto scaled = vector * TypeParam{2};
  EXPECT_EQ(scaled.x(), TypeParam{4});
  EXPECT_EQ(scaled.y(), TypeParam{8});
  EXPECT_EQ(scaled.z(), TypeParam{12});

  const auto halved = vector / TypeParam{2};
  EXPECT_EQ(halved.x(), TypeParam{1});
  EXPECT_EQ(halved.y(), TypeParam{2});
  EXPECT_EQ(halved.z(), TypeParam{3});
}

TYPED_TEST(vector3_algebra_test, dot_product_matches_definition) {
  using vector3 = basic_vector3<TypeParam>;

  const auto lhs = vector3{TypeParam{1}, TypeParam{2}, TypeParam{3}};
  const auto rhs = vector3{TypeParam{4}, TypeParam{5}, TypeParam{6}};

  // 1*4 + 2*5 + 3*6 = 32
  EXPECT_FLOAT_EQ(vector3::dot(lhs, rhs), 32.0f);
}

TYPED_TEST(vector3_algebra_test, cross_product_follows_right_hand_rule) {
  using vector3 = basic_vector3<TypeParam>;

  const auto x = vector3{TypeParam{1}, TypeParam{0}, TypeParam{0}};
  const auto y = vector3{TypeParam{0}, TypeParam{1}, TypeParam{0}};

  const auto z = vector3::cross(x, y);

  EXPECT_EQ(z.x(), TypeParam{0});
  EXPECT_EQ(z.y(), TypeParam{0});
  EXPECT_EQ(z.z(), TypeParam{1});

  // Anticommutativity: a x b == -(b x a)
  const auto reversed = vector3::cross(y, x);
  EXPECT_EQ(reversed.x(), -z.x());
  EXPECT_EQ(reversed.y(), -z.y());
  EXPECT_EQ(reversed.z(), -z.z());
}

// -- Floating-point-only operations: length, normalization, distance, reflection.

template<typename Type>
class vector3_floating_test : public ::testing::Test { };

using floating_scalar_types = ::testing::Types<std::float_t, std::double_t>;
TYPED_TEST_SUITE(vector3_floating_test, floating_scalar_types);

TYPED_TEST(vector3_floating_test, length_and_length_squared) {
  using vector3 = basic_vector3<TypeParam>;

  const auto vector = vector3{TypeParam{3}, TypeParam{4}, TypeParam{0}};

  EXPECT_NEAR(static_cast<std::double_t>(vector.length_squared()), 25.0, 1e-4);
  EXPECT_NEAR(static_cast<std::double_t>(vector.length()), 5.0, 1e-4);
}

TYPED_TEST(vector3_floating_test, normalized_produces_unit_length_vector) {
  using vector3 = basic_vector3<TypeParam>;

  const auto vector = vector3{TypeParam{3}, TypeParam{4}, TypeParam{0}};
  const auto normalized = vector3::normalized(vector);

  EXPECT_NEAR(static_cast<std::double_t>(normalized.length()), 1.0, 1e-4);
}

TYPED_TEST(vector3_floating_test, normalized_of_zero_vector_is_left_untouched) {
  using vector3 = basic_vector3<TypeParam>;

  const auto normalized = vector3::normalized(vector3::zero);

  EXPECT_EQ(normalized, vector3::zero);
}

TYPED_TEST(vector3_floating_test, distance_matches_length_of_difference) {
  using vector3 = basic_vector3<TypeParam>;

  const auto lhs = vector3{TypeParam{0}, TypeParam{0}, TypeParam{0}};
  const auto rhs = vector3{TypeParam{3}, TypeParam{4}, TypeParam{0}};

  EXPECT_NEAR(static_cast<std::double_t>(vector3::distance(lhs, rhs)), 5.0, 1e-4);
  EXPECT_NEAR(static_cast<std::double_t>(vector3::distance_squared(lhs, rhs)), 25.0, 1e-4);
}

TYPED_TEST(vector3_floating_test, reflect_mirrors_vector_across_normal) {
  using vector3 = basic_vector3<TypeParam>;

  // Incoming vector hitting a flat surface whose normal points straight up.
  const auto incoming = vector3{TypeParam{1}, TypeParam{-1}, TypeParam{0}};
  const auto normal = vector3{TypeParam{0}, TypeParam{1}, TypeParam{0}};

  const auto reflected = vector3::reflect(incoming, normal);

  EXPECT_NEAR(static_cast<std::double_t>(reflected.x()), 1.0, 1e-4);
  EXPECT_NEAR(static_cast<std::double_t>(reflected.y()), 1.0, 1e-4);
  EXPECT_NEAR(static_cast<std::double_t>(reflected.z()), 0.0, 1e-4);
}
