// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <tests/vector2_tests.hpp>
#include <tests/vector3_tests.hpp>
#include <tests/vector4_tests.hpp>

#include <tests/matrix4x4_tests.hpp>

#include <tests/quaternion_tests.hpp>

#include <tests/angle_tests.hpp>

auto main(int argc, char* argv[]) -> int {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
