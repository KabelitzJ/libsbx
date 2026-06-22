// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <compare>
#include <type_traits>

#include <libsbx/units/v2/dimension.hpp>
#include <libsbx/units/v2/quantity.hpp>

using namespace sbx::units::v2;
using namespace sbx::units::v2::literals;

TEST(units, default_construct_is_zero) {
  EXPECT_FLOAT_EQ(meters{}, 0.0f);
}

TEST(units, value_construct) {
  EXPECT_FLOAT_EQ(meters{3.5f}, 3.5f);
}

TEST(units, convert_km_to_m) {
  const auto m = meters{kilometers{1.0f}};
  EXPECT_FLOAT_EQ(m, 1000.0f);
}

TEST(units, convert_m_to_mm) {
  const auto mm = millimeters{meters{1.0f}};
  EXPECT_FLOAT_EQ(mm, 1000.0f);
}

TEST(units, convert_m_to_km) {
  const auto km = kilometers{meters{2000.0f}};
  EXPECT_FLOAT_EQ(km, 2.0f);
}

TEST(units, add_same_scale) {
  EXPECT_FLOAT_EQ((meters{1.0f} + meters{2.0f}), 3.0f);
}

TEST(units, add_different_scale_result_in_lhs_scale) {
  const auto result = meters{1.0f} + kilometers{1.0f};

  static_assert(std::is_same_v<decltype(result), const meters>);

  EXPECT_FLOAT_EQ(result, 1001.0f);
}

TEST(units, subtract_different_scale) {
  EXPECT_FLOAT_EQ((meters{2000.0f} - kilometers{1.0f}), 1000.0f);
}

TEST(units, unary_minus) {
  EXPECT_FLOAT_EQ((-meters{4.0f}), -4.0f);
}

TEST(units, multiply_produces_area_dimension) {
  const auto a = meters{2.0f} * meters{3.0f};

  static_assert(std::is_same_v<typename decltype(a)::dimension_type, area_dimension>);

  EXPECT_FLOAT_EQ(a, 6.0f);
}

TEST(units, divide_produces_velocity_dimension) {
  const auto v = meters{10.0f} / seconds{2.0f};

  static_assert(std::is_same_v<typename decltype(v)::dimension_type, velocity_dimension>);
  static_assert(std::is_same_v<decltype(v), const meters_per_second>);

  EXPECT_FLOAT_EQ(v, 5.0f);
}

TEST(units, scalar_multiply) {
  EXPECT_FLOAT_EQ((meters{2.0f} * 3.0f), 6.0f);
}

TEST(units, scalar_multiply_commutative) {
  EXPECT_FLOAT_EQ((3.0f * meters{2.0f}), 6.0f);
}

TEST(units, scalar_divide) {
  EXPECT_FLOAT_EQ((meters{6.0f} / 2.0f), 3.0f);
}

TEST(units, equality_across_scales) {
  EXPECT_TRUE(meters{1000.0f} == kilometers{1.0f});
  EXPECT_FALSE(meters{999.0f} == kilometers{1.0f});
}

TEST(units, three_way_ordering) {
  EXPECT_TRUE((meters{1.0f} <=> meters{2.0f}) < 0);
  EXPECT_TRUE((meters{2.0f} <=> meters{1.0f}) > 0);
  EXPECT_TRUE((meters{1.0f} <=> meters{1.0f}) == 0);
}

TEST(units, three_way_across_scales) {
  EXPECT_TRUE((meters{1000.0f} <=> kilometers{1.0f}) == 0);
}

TEST(units, literals_length) {
  EXPECT_FLOAT_EQ((2.5_m), 2.5f);
  EXPECT_FLOAT_EQ((3.0_km), 3.0f);
  EXPECT_FLOAT_EQ((7.0_cm), 7.0f);
}

TEST(units, literals_time) {
  EXPECT_FLOAT_EQ((5.0_ms), 5.0f);  // fails if _ms returns seconds{}
  EXPECT_FLOAT_EQ((2.0_s), 2.0f);
}

TEST(units, literals_mass) {
  EXPECT_FLOAT_EQ((4.0_kg), 4.0f);
}
