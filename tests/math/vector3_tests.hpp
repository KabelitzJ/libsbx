// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_VECTOR3_TESTS_HPP_
#define LIBSBX_MATH_VECTOR3_TESTS_HPP_

#include <sstream>

#include <gtest/gtest.h>

#include <libsbx/math/vector3.hpp>

TEST(libsbx_math_vector3, zero) {
  const auto vector = sbx::math::vector3::zero;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, one) {
  const auto vector = sbx::math::vector3::one;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
}

TEST(libsbx_math_vector3, right) {
  const auto vector = sbx::math::vector3::right;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, left) {
  const auto vector = sbx::math::vector3::left;

  EXPECT_FLOAT_EQ(vector.x(), -1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, up) {
  const auto vector = sbx::math::vector3::up;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, down) {
  const auto vector = sbx::math::vector3::down;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), -1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, forward) {
  const auto vector = sbx::math::vector3::forward;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), -1.0f);
}

TEST(libsbx_math_vector3, backward) {
  const auto vector = sbx::math::vector3::backward;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
}

TEST(libsbx_math_vector3, default_constructor) {
  const auto vector = sbx::math::vector3{};

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, base_constructor) {
  const auto base = sbx::math::basic_vector<3u, std::float_t>{1};
  const auto vector = sbx::math::vector3{base};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
}

TEST(libsbx_math_vector3, single_value_constructor) {
  const auto vector = sbx::math::vector3{1};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
}

TEST(libsbx_math_vector3, multi_value_constructor) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

TEST(libsbx_math_vector3, vector2_constructor) {
  const auto vector = sbx::math::vector3{sbx::math::vector2{1, 2}};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);

  const auto other = sbx::math::vector3{sbx::math::vector2{1, 2}, 3};

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
}

TEST(libsbx_math_vector3, copy_constructor) {
  const auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = vector;

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
}

TEST(libsbx_math_vector3, move_constructor) {
  auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = std::move(vector);

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
}

TEST(libsbx_math_vector3, cross_product) {
  const auto lhs = sbx::math::vector3{1, 2, 3};
  const auto rhs = sbx::math::vector3{4, 5, 6};

  const auto result = sbx::math::vector3::cross(lhs, rhs);

  EXPECT_FLOAT_EQ(result.x(), -3.0f);
  EXPECT_FLOAT_EQ(result.y(), 6.0f);
  EXPECT_FLOAT_EQ(result.z(), -3.0f);
}

TEST(libsbx_math_vector3, dot_product) {
  const auto lhs = sbx::math::vector3{1, 2, 3};
  const auto rhs = sbx::math::vector3{4, 5, 6};

  const auto result = sbx::math::vector3::dot(lhs, rhs);

  EXPECT_FLOAT_EQ(result, 32.0f);
}

TEST(libsbx_math_vector3, normalized) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  const auto result = sbx::math::vector3::normalized(vector);

  EXPECT_FLOAT_EQ(result.x(), 0.26726124191242438468455348087975f);
  EXPECT_FLOAT_EQ(result.y(), 0.5345224838248487693691069617595f);
  EXPECT_FLOAT_EQ(result.z(), 0.80178372573727315405366044263925f);
  EXPECT_FLOAT_EQ(result.length(), 1.0f);
}

TEST(libsbx_math_vector3, conversion) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  const auto result = sbx::math::vector2{vector};

  EXPECT_FLOAT_EQ(result.x(), 1.0f);
  EXPECT_FLOAT_EQ(result.y(), 2.0f);
}

TEST(libsbx_math_vector3, copy_assignment) {
  auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  vector = other;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

TEST(libsbx_math_vector3, move_assignment) {
  auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  vector = std::move(other);

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

TEST(libsbx_math_vector3, addition_assignment) {
  auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  vector += other;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
  EXPECT_FLOAT_EQ(vector.y(), 4.0f);
  EXPECT_FLOAT_EQ(vector.z(), 6.0f);
}

TEST(libsbx_math_vector3, subtraction_assignment) {
  auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  vector -= other;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
}

TEST(libsbx_math_vector3, multiplication_assignment) {
  auto vector = sbx::math::vector3{1, 2, 3};

  vector *= 2;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
  EXPECT_FLOAT_EQ(vector.y(), 4.0f);
  EXPECT_FLOAT_EQ(vector.z(), 6.0f);
}

TEST(libsbx_math_vector3, division_assignment) {
  auto vector = sbx::math::vector3{2, 4, 6};

  vector /= 2;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

TEST(libsbx_math_vector3, equality) {
  const auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  EXPECT_TRUE(vector == other);
}

TEST(libsbx_math_vector3, inequality) {
  const auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{3, 2, 1};

  EXPECT_TRUE(vector != other);
}

TEST(libsbx_math_vector3, addition) {
  const auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  const auto result = vector + other;

  EXPECT_FLOAT_EQ(result.x(), 2.0f);
  EXPECT_FLOAT_EQ(result.y(), 4.0f);
  EXPECT_FLOAT_EQ(result.z(), 6.0f);
}

TEST(libsbx_math_vector3, subtraction) {
  const auto vector = sbx::math::vector3{1, 2, 3};
  const auto other = sbx::math::vector3{1, 2, 3};

  const auto result = vector - other;

  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_FLOAT_EQ(result.z(), 0.0f);
}

TEST(libsbx_math_vector3, negation) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  const auto result = -vector;

  EXPECT_FLOAT_EQ(result.x(), -1.0f);
  EXPECT_FLOAT_EQ(result.y(), -2.0f);
  EXPECT_FLOAT_EQ(result.z(), -3.0f);
}

TEST(libsbx_math_vector3, multiplication) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  const auto result = vector * 2;

  EXPECT_FLOAT_EQ(result.x(), 2.0f);
  EXPECT_FLOAT_EQ(result.y(), 4.0f);
  EXPECT_FLOAT_EQ(result.z(), 6.0f);
}

TEST(libsbx_math_vector3, division) {
  const auto vector = sbx::math::vector3{2, 4, 6};

  const auto result = vector / 2;

  EXPECT_FLOAT_EQ(result.x(), 1.0f);
  EXPECT_FLOAT_EQ(result.y(), 2.0f);
  EXPECT_FLOAT_EQ(result.z(), 3.0f);
}

TEST(libsbx_math_vector3, length_squared) {
  const auto vector = sbx::math::vector3{2, 4, 6};

  EXPECT_FLOAT_EQ(vector.length_squared(), 56.0f);
}

TEST(libsbx_math_vector3, length) {
  const auto vector = sbx::math::vector3{2, 4, 6};

  EXPECT_FLOAT_EQ(vector.length(), 7.4833147735478831617390654696346f);
}

TEST(libsbx_math_vector3, get_x) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
}

TEST(libsbx_math_vector3, set_x) {
  auto vector = sbx::math::vector3{1, 2, 3};

  vector.x() = 2;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
}

TEST(libsbx_math_vector3, get_y) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
}

TEST(libsbx_math_vector3, set_y) {
  auto vector = sbx::math::vector3{1, 2, 3};

  vector.y() = 3;

  EXPECT_FLOAT_EQ(vector.y(), 3.0f);
}

TEST(libsbx_math_vector3, get_z) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

TEST(libsbx_math_vector3, set_z) {
  auto vector = sbx::math::vector3{1, 2, 3};

  vector.z() = 4;

  EXPECT_FLOAT_EQ(vector.z(), 4.0f);
}

TEST(libsbx_math_vector3, hash) {
  const auto first = sbx::math::vector3{1, 2, 3};
  const auto second = sbx::math::vector3{1, 2, 3};

  EXPECT_EQ(std::hash<sbx::math::vector3>{}(first), std::hash<sbx::math::vector3>{}(second));
}

TEST(libsbx_math_vector3, formatting) {
  const auto float_vector = sbx::math::vector3f{1, 2, 3};

  auto formatted = fmt::format("{}", float_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1.00, y: 2.00, z: 3.00}");

  const auto int_vector = sbx::math::vector3i{1, 2, 3};

  formatted = fmt::format("{}", int_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1, y: 2, z: 3}");
}

TEST(libsbx_math_vector3, serialize) {
  const auto vector = sbx::math::vector3{1, 2, 3};

  const auto serialized = YAML::Node{vector};

  EXPECT_FLOAT_EQ(serialized["x"].as<std::float_t>(), 1.0f);
  EXPECT_FLOAT_EQ(serialized["y"].as<std::float_t>(), 2.0f);
  EXPECT_FLOAT_EQ(serialized["z"].as<std::float_t>(), 3.0f);

  auto steam = std::stringstream{};

  steam << serialized;

  EXPECT_STREQ(steam.str().c_str(), "{x: 1, y: 2, z: 3}");
}

TEST(libsbx_math_vector3, deserialize) {
  const auto node = YAML::Load("{x: 1, y: 2, z: 3}");

  const auto vector = node.as<sbx::math::vector3>();

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
}

#endif // LIBSBX_MATH_VECTOR3_TESTS_HPP_
