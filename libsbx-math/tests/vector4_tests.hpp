// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TESTS_VECTOR4_TESTS_HPP_
#define LIBSBX_MATH_TESTS_VECTOR4_TESTS_HPP_

#include <sstream>

#include <gtest/gtest.h>

#include <libsbx/math/vector4.hpp>

TEST(libsbx_math_vector4, zero) {
  const auto vector = sbx::math::vector4::zero;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
  EXPECT_FLOAT_EQ(vector.w(), 0.0f);
}

TEST(libsbx_math_vector4, one) {
  const auto vector = sbx::math::vector4::one;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
  EXPECT_FLOAT_EQ(vector.w(), 1.0f);
}

TEST(libsbx_math_vector4, default_constructor) {
  const auto vector = sbx::math::vector4{};

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
  EXPECT_FLOAT_EQ(vector.w(), 0.0f);
}

TEST(libsbx_math_vector4, base_constructor) {
  const auto base = sbx::math::basic_vector<4u, std::float_t>{1};
  const auto vector = sbx::math::vector4{base};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
  EXPECT_FLOAT_EQ(vector.w(), 1.0f);
}

TEST(libsbx_math_vector4, single_value_constructor) {
  const auto vector = sbx::math::vector4{1};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 1.0f);
  EXPECT_FLOAT_EQ(vector.z(), 1.0f);
  EXPECT_FLOAT_EQ(vector.w(), 1.0f);
}

TEST(libsbx_math_vector4, multi_value_constructor) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
  EXPECT_FLOAT_EQ(vector.w(), 4.0f);
}

TEST(libsbx_math_vector4, vector3_constructor) {
  const auto vector = sbx::math::vector4{sbx::math::vector3{1, 2, 3}};

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
  EXPECT_FLOAT_EQ(vector.w(), 0.0f);

  const auto other = sbx::math::vector4{sbx::math::vector3{1, 2, 3}, 4};

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
  EXPECT_FLOAT_EQ(other.w(), 4.0f);
}

TEST(libsbx_math_vector4, dot_product) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};
  const auto rhs = sbx::math::vector4{4, 3, 2, 1};

  const auto result = sbx::math::vector4::dot(lhs, rhs);

  EXPECT_FLOAT_EQ(result, 20.0f);
}

TEST(libsbx_math_vector4, normalized) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto result = sbx::math::vector4::normalized(vector);

  EXPECT_FLOAT_EQ(result.x(), 0.1825741858350553711523232609336f);
  EXPECT_FLOAT_EQ(result.y(), 0.3651483716701107423046465218672f);
  EXPECT_FLOAT_EQ(result.z(), 0.5477225575051661134569697828008f);
  EXPECT_FLOAT_EQ(result.w(), 0.7302967433402214846092930437344f);
  EXPECT_FLOAT_EQ(result.length(), 1.0f);
}

TEST(libsbx_math_vector4, conversion) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto result = sbx::math::vector3{vector};

  EXPECT_FLOAT_EQ(result.x(), 1.0f);
  EXPECT_FLOAT_EQ(result.y(), 2.0f);
  EXPECT_FLOAT_EQ(result.z(), 3.0f);
}

TEST(libsbx_math_vector4, copy_constructor) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto other = vector;

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
  EXPECT_FLOAT_EQ(other.w(), 4.0f);
}

TEST(libsbx_math_vector4, move_constructor) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto other = std::move(vector);

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
  EXPECT_FLOAT_EQ(other.w(), 4.0f);
}

TEST(libsbx_math_vector4, copy_assignment) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};
  auto other = sbx::math::vector4{};
  other = vector;

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
  EXPECT_FLOAT_EQ(other.w(), 4.0f);
}

TEST(libsbx_math_vector4, move_assignment) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};
  auto other = sbx::math::vector4{};
  other = std::move(vector);

  EXPECT_FLOAT_EQ(other.x(), 1.0f);
  EXPECT_FLOAT_EQ(other.y(), 2.0f);
  EXPECT_FLOAT_EQ(other.z(), 3.0f);
  EXPECT_FLOAT_EQ(other.w(), 4.0f);
}

TEST(libsbx_math_vector4, addition_assignment) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto other = sbx::math::vector4{1, 2, 3, 4};

  vector += other;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
  EXPECT_FLOAT_EQ(vector.y(), 4.0f);
  EXPECT_FLOAT_EQ(vector.z(), 6.0f);
  EXPECT_FLOAT_EQ(vector.w(), 8.0f);
}

TEST(libsbx_math_vector4, subtraction_assignment) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto other = sbx::math::vector4{1, 2, 3, 4};

  vector -= other;

  EXPECT_FLOAT_EQ(vector.x(), 0.0f);
  EXPECT_FLOAT_EQ(vector.y(), 0.0f);
  EXPECT_FLOAT_EQ(vector.z(), 0.0f);
  EXPECT_FLOAT_EQ(vector.w(), 0.0f);
}

TEST(libsbx_math_vector4, multiplication_assignment) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};

  vector *= 2;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
  EXPECT_FLOAT_EQ(vector.y(), 4.0f);
  EXPECT_FLOAT_EQ(vector.z(), 6.0f);
  EXPECT_FLOAT_EQ(vector.w(), 8.0f);
}

TEST(libsbx_math_vector4, division_assignment) {
  auto vector = sbx::math::vector4{2, 4, 6, 8};

  vector /= 2;

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
  EXPECT_FLOAT_EQ(vector.w(), 4.0f);
}

TEST(libsbx_math_vector4, equality) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};
  const auto rhs = sbx::math::vector4{1, 2, 3, 4};

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);
}

TEST(libsbx_math_vector4, inequality) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};
  const auto rhs = sbx::math::vector4{4, 3, 2, 1};

  EXPECT_TRUE(lhs != rhs);
  EXPECT_FALSE(lhs == rhs);
}

TEST(libsbx_math_vector4, addition) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};
  const auto rhs = sbx::math::vector4{4, 3, 2, 1};
  const auto result = lhs + rhs;

  EXPECT_FLOAT_EQ(result.x(), 5.0f);
  EXPECT_FLOAT_EQ(result.y(), 5.0f);
  EXPECT_FLOAT_EQ(result.z(), 5.0f);
  EXPECT_FLOAT_EQ(result.w(), 5.0f);
}

TEST(libsbx_math_vector4, subtraction) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};
  const auto rhs = sbx::math::vector4{4, 3, 2, 1};
  const auto result = lhs - rhs;

  EXPECT_FLOAT_EQ(result.x(), -3.0f);
  EXPECT_FLOAT_EQ(result.y(), -1.0f);
  EXPECT_FLOAT_EQ(result.z(), 1.0f);
  EXPECT_FLOAT_EQ(result.w(), 3.0f);
}

TEST(libsbx_math_vector4, negation) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};
  const auto result = -vector;

  EXPECT_FLOAT_EQ(result.x(), -1.0f);
  EXPECT_FLOAT_EQ(result.y(), -2.0f);
  EXPECT_FLOAT_EQ(result.z(), -3.0f);
  EXPECT_FLOAT_EQ(result.w(), -4.0f);
}

TEST(libsbx_math_vector4, multiplication) {
  const auto lhs = sbx::math::vector4{1, 2, 3, 4};

  const auto result = lhs * 2.0f;

  EXPECT_FLOAT_EQ(result.x(), 2.0f);
  EXPECT_FLOAT_EQ(result.y(), 4.0f);
  EXPECT_FLOAT_EQ(result.z(), 6.0f);
  EXPECT_FLOAT_EQ(result.w(), 8.0f);
}

TEST(libsbx_math_vector4, division) {
  const auto lhs = sbx::math::vector4{4, 6, 6, 4};
  
  const auto result = lhs / 2.0f;

  EXPECT_FLOAT_EQ(result.x(), 2.0f);
  EXPECT_FLOAT_EQ(result.y(), 3.0f);
  EXPECT_FLOAT_EQ(result.z(), 3.0f);
  EXPECT_FLOAT_EQ(result.w(), 2.0f);
}

TEST(libsbx_math_vector4, length_squared) {
  const auto vector = sbx::math::vector4{2, 4, 6, 8};

  const auto result = vector.length_squared();

  EXPECT_FLOAT_EQ(result, 120.0f);
}

TEST(libsbx_math_vector4, length) {
  const auto vector = sbx::math::vector4{2, 4, 6, 8};

  const auto result = vector.length();

  EXPECT_FLOAT_EQ(result, 10.954451150103322f);
}

TEST(libsbx_math_vector4, normalize) {
  auto vector = sbx::math::vector4{2, 4, 6, 8};

  vector.normalize();

  EXPECT_FLOAT_EQ(vector.x(), 0.1825741858350553711523232609336f);
  EXPECT_FLOAT_EQ(vector.y(), 0.3651483716701107423046465218672f);
  EXPECT_FLOAT_EQ(vector.z(), 0.5477225575051661134569697828008f);
  EXPECT_FLOAT_EQ(vector.w(), 0.7302967433402214846092930437344f);
  EXPECT_FLOAT_EQ(vector.length(), 1.0f);
}

TEST(libsbx_math_vector4, get_x) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto result = vector.x();

  EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(libsbx_math_vector4, set_x) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};

  vector.x() = 2;

  EXPECT_FLOAT_EQ(vector.x(), 2.0f);
}

TEST(libsbx_math_vector4, get_y) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto result = vector.y();

  EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(libsbx_math_vector4, set_y) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};

  vector.y() = 3;

  EXPECT_FLOAT_EQ(vector.y(), 3.0f);
}

TEST(libsbx_math_vector4, get_z) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto result = vector.z();

  EXPECT_FLOAT_EQ(result, 3.0f);
}

TEST(libsbx_math_vector4, set_z) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};

  vector.z() = 4;

  EXPECT_FLOAT_EQ(vector.z(), 4.0f);
}

TEST(libsbx_math_vector4, get_w) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto result = vector.w();

  EXPECT_FLOAT_EQ(result, 4.0f);
}

TEST(libsbx_math_vector4, set_w) {
  auto vector = sbx::math::vector4{1, 2, 3, 4};

  vector.w() = 5;

  EXPECT_FLOAT_EQ(vector.w(), 5.0f);
}

TEST(libsbx_math_vector4, hash) {
  const auto first = sbx::math::vector4{1, 2, 3, 4};
  const auto second = sbx::math::vector4{1, 2, 3, 4};

  EXPECT_EQ(std::hash<sbx::math::vector4>{}(first), std::hash<sbx::math::vector4>{}(second));
}

TEST(libsbx_math_vector4, formatting) {
  const auto float_vector = sbx::math::vector4f{1, 2, 3, 4};

  auto formatted = fmt::format("{}", float_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1.00, y: 2.00, z: 3.00, w: 4.00}");

  const auto int_vector = sbx::math::vector4i{1, 2, 3, 4};

  formatted = fmt::format("{}", int_vector);

  EXPECT_STREQ(formatted.c_str(), "{x: 1, y: 2, z: 3, w: 4}");
}

TEST(libsbx_math_vector4, serialize) {
  const auto vector = sbx::math::vector4{1, 2, 3, 4};

  const auto serialized = YAML::Node{vector};

  EXPECT_FLOAT_EQ(serialized["x"].as<std::float_t>(), 1.0f);
  EXPECT_FLOAT_EQ(serialized["y"].as<std::float_t>(), 2.0f);
  EXPECT_FLOAT_EQ(serialized["z"].as<std::float_t>(), 3.0f);
  EXPECT_FLOAT_EQ(serialized["w"].as<std::float_t>(), 4.0f);

  auto steam = std::stringstream{};

  steam << serialized;

  EXPECT_STREQ(steam.str().c_str(), "{x: 1, y: 2, z: 3, w: 4}");
}

TEST(libsbx_math_vector4, deserialize) {
  const auto node = YAML::Load("{x: 1, y: 2, z: 3, w: 4}");

  const auto vector = node.as<sbx::math::vector4>();

  EXPECT_FLOAT_EQ(vector.x(), 1.0f);
  EXPECT_FLOAT_EQ(vector.y(), 2.0f);
  EXPECT_FLOAT_EQ(vector.z(), 3.0f);
  EXPECT_FLOAT_EQ(vector.w(), 4.0f);
}

#endif // LIBSBX_MATH_TESTS_VECTOR4_TESTS_HPP_
