// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TESTS_VECTOR2_TESTS_HPP_
#define LIBSBX_MATH_TESTS_VECTOR2_TESTS_HPP_

#include <sstream>

#include <gtest/gtest.h>

#include <libsbx/math/vector2.hpp>

TEST(libsbx_math_vector2, zero) {
  const auto vector = sbx::math::vector2::zero;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
}

TEST(libsbx_math_vector2, one) {
  const auto vector = sbx::math::vector2::one;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
}

TEST(libsbx_math_vector2, default_constructor) {
  const auto vector = sbx::math::vector2{};

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
}

TEST(libsbx_math_vector2, base_constructor) {
  const auto base = sbx::math::basic_vector<2u, std::float_t>{1};
  const auto vector = sbx::math::vector2{base};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
}

TEST(libsbx_math_vector2, single_value_constructor) {
  const auto vector = sbx::math::vector2{1};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
}

TEST(libsbx_math_vector2, multi_value_constructor) {
  const auto vector = sbx::math::vector2{1, 2};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
}

TEST(libsbx_math_vector2, copy_constructor) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto other = vector;

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
}

TEST(libsbx_math_vector2, move_constructor) {
  auto vector = sbx::math::vector2{1, 2};
  const auto other = std::move(vector);

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
}

TEST(libsbx_math_vector2, copy_assignment) {
  const auto vector = sbx::math::vector2{1, 2};
  auto other = sbx::math::vector2{};
  other = vector;

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
}

TEST(libsbx_math_vector2, move_assignment) {
  auto vector = sbx::math::vector2{1, 2};
  auto other = sbx::math::vector2{};
  other = std::move(vector);

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
}

TEST(libsbx_math_vector2, equality) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = sbx::math::vector2{1, 2};

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);
}

TEST(libsbx_math_vector2, inequality) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = sbx::math::vector2{2, 1};

  EXPECT_TRUE(lhs != rhs);
  EXPECT_FALSE(lhs == rhs);
}

TEST(libsbx_math_vector2, addition_assignment) {
  auto vector = sbx::math::vector2{1, 2};
  vector += sbx::math::vector2{2, 1};

  EXPECT_FLOAT_EQ(vector.x(), 3.0f);
  EXPECT_FLOAT_EQ(vector.y(), 3.0f);
}

TEST(libsbx_math_vector2, subtraction_assignment) {
  auto vector = sbx::math::vector2{1, 2};
  vector -= sbx::math::vector2{2, 1};

  EXPECT_FLOAT_EQ(vector.x(), -1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
}

TEST(libsbx_math_vector2, multiplication_assignment) {
  auto vector = sbx::math::vector2{1, 2};
  vector *= 2;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
  EXPECT_FLOAT_EQ(vector.y(), 4.0f);
}

TEST(libsbx_math_vector2, division_assignment) {
  auto vector = sbx::math::vector2{2, 4};
  vector /= 2;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
}

TEST(libsbx_math_vector2, addition) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = sbx::math::vector2{2, 1};
  const auto result = lhs + rhs;

  EXPECT_FLOAT_EQ(result.x(), 3.0f);
  EXPECT_FLOAT_EQ(result.y(), 3.0f);
}

TEST(libsbx_math_vector2, subtraction) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = sbx::math::vector2{2, 1};
  const auto result = lhs - rhs;

  EXPECT_FLOAT_EQ(result.x(), -1.0f);
  EXPECT_FLOAT_EQ(result.y(), 1.0f);
}

TEST(libsbx_math_vector2, multiplication) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = 2;
  const auto result = lhs * rhs;

  EXPECT_FLOAT_EQ(result.x(), 2.0f);
  EXPECT_FLOAT_EQ(result.y(), 4.0f);
}

TEST(libsbx_math_vector2, division) {
  const auto lhs = sbx::math::vector2{2, 4};
  const auto rhs = 2;
  const auto result = lhs / rhs;

  EXPECT_FLOAT_EQ(result.x(), 1.0f);
  EXPECT_FLOAT_EQ(result.y(), 2.0f);
}

TEST(libsbx_math_vector2, negation) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = -vector;

  EXPECT_FLOAT_EQ(result.x(), -1.0f);
  EXPECT_FLOAT_EQ(result.y(), -2.0f);
}

TEST(libsbx_math_vector2, dot) {
  const auto lhs = sbx::math::vector2{1, 2};
  const auto rhs = sbx::math::vector2{2, 1};
  const auto result = sbx::math::vector2::dot(lhs, rhs);

  EXPECT_FLOAT_EQ(result, 4.0f);
}

TEST(libsbx_math_vector2, normalized) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = sbx::math::vector2::normalized(vector);

  EXPECT_FLOAT_EQ(result.x(), 1.0f / std::sqrt(5.0f));
  EXPECT_FLOAT_EQ(result.y(), 2.0f / std::sqrt(5.0f));
}

TEST(libsbx_math_vector2, length_squared) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = vector.length_squared();

  EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(libsbx_math_vector2, length) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = vector.length();

  EXPECT_FLOAT_EQ(result, std::sqrt(5.0f));
}

TEST(libsbx_math_vector2, normalize) {
  auto vector = sbx::math::vector2{1, 2};
  vector.normalize();

  EXPECT_FLOAT_EQ(vector.x(), 1.0f / std::sqrt(5.0f));
  EXPECT_FLOAT_EQ(vector.y(), 2.0f / std::sqrt(5.0f));
}

TEST(libsbx_math_vector2, get_x) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = vector.x();

  EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(libsbx_math_vector2, set_x) {
  auto vector = sbx::math::vector2{1, 2};
  vector.x() = 3;

  EXPECT_FLOAT_EQ(vector.x(), 3.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
}

TEST(libsbx_math_vector2, get_y) {
  const auto vector = sbx::math::vector2{1, 2};
  const auto result = vector.y();

  EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(libsbx_math_vector2, set_y) {
  auto vector = sbx::math::vector2{1, 2};
  vector.y() = 3;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 3.0f);
}

TEST(libsbx_math_vector2, hash) {
  const auto first = sbx::math::vector2{1, 2};
  const auto second = sbx::math::vector2{1, 2};

  EXPECT_EQ(std::hash<sbx::math::vector2>{}(first), std::hash<sbx::math::vector2>{}(second));
}

TEST(libsbx_math_vector2, formatting) {
  const auto float_vector = sbx::math::vector2f{1, 2};

  auto formatted = fmt::format("{}", float_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1.00, y: 2.00}");

  const auto int_vector = sbx::math::vector2i{1, 2};

  formatted = fmt::format("{}", int_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1, y: 2}");
}

TEST(libsbx_math_vector2, serialize) {
  const auto vector = sbx::math::vector2{1, 2};

  const auto serialized = YAML::Node{vector};

  EXPECT_FLOAT_EQ(serialized["x"].as<std::float_t>(), 1.0f);
  EXPECT_FLOAT_EQ(serialized["y"].as<std::float_t>(), 2.0f);

  auto steam = std::stringstream{};

  steam << serialized;

  EXPECT_STREQ(steam.str().c_str(), "{x: 1, y: 2}");
}

TEST(libsbx_math_vector2, deserialize) {
  const auto node = YAML::Load("{x: 1, y: 2}");

  const auto vector = node.as<sbx::math::vector2>();

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
}

#endif // LIBSBX_MATH_TESTS_VECTOR2_TESTS_HPP_
