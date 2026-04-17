// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include <libsbx/reflection/reflection.hpp>

class test_struct {

  friend struct sbx::reflection::description<test_struct>;

public:

  test_struct(std::int32_t a, std::float_t b, std::string c)
  : _a{a}, 
    _b{b}, 
    _c{std::move(c)} {}

private:

  std::int32_t _a;
  std::float_t _b;
  std::string _c;

}; // class test_struct

template<>
struct sbx::reflection::description<test_struct> {

  static constexpr auto name() -> std::string_view {
    return "test_struct";
  }

  static constexpr auto members() {
    return std::make_tuple(
      member{"a", &test_struct::_a},
      member{"b", &test_struct::_b},
      member{"c", &test_struct::_c}
    );
  }

}; // struct sbx::reflection::description

enum class test_enum {
  one,
  two,
  three
}; // enum class test_enum

template<>
struct sbx::reflection::description<test_enum> {

  static constexpr auto name() -> std::string_view {
    return "test_enum";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"one", test_enum::one},
      enumerator{"two", test_enum::two},
      enumerator{"three", test_enum::three}
    );
  }

}; // struct sbx::reflection::description

TEST(reflection, iterates_members) {
  auto instance = test_struct{42, 3.14f, "hello"};
  auto visited = 0;

  sbx::reflection::for_each(instance, [&](auto name, auto const& value) {
    ++visited;
    (void)name;
    (void)value;
  });

  EXPECT_EQ(visited, 3);
  EXPECT_EQ(sbx::reflection::description<test_struct>::name(), "test_struct");
}

TEST(reflection, visit_member_finds_and_reads) {
  const auto instance = test_struct{42, 3.14f, "hello"};

  auto seen_a = std::int32_t{0};

  const auto found = sbx::reflection::visit_member(const_cast<test_struct&>(instance), "a", [&](auto& value) {
    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::int32_t>) {
      seen_a = value;
    }
  });

  EXPECT_TRUE(found);
  EXPECT_EQ(seen_a, 42);
}

TEST(reflection, visit_member_writes) {
  auto instance = test_struct{0, 0.0f, ""};

  const auto found = sbx::reflection::visit_member(instance, "b", [](auto& value) {
    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::float_t>) {
      value = 2.71f;
    }
  });

  EXPECT_TRUE(found);

  auto seen_b = std::float_t{0.0f};

  sbx::reflection::visit_member(instance, "b", [&](auto& value) {
    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::float_t>) {
      seen_b = value;
    }
  });

  EXPECT_FLOAT_EQ(seen_b, 2.71f);
}

TEST(reflection, visit_member_returns_false_on_missing) {
  auto instance = test_struct{42, 3.14f, "hello"};
  auto called = false;

  const auto found = sbx::reflection::visit_member(instance, "does_not_exist", [&](auto&) {
    called = true;
  });

  EXPECT_FALSE(found);
  EXPECT_FALSE(called);
}

TEST(reflection, visit_member_matches_each_field) {
  auto instance = test_struct{42, 3.14f, "hello"};

  EXPECT_TRUE(sbx::reflection::visit_member(instance, "a", [](auto&) {}));
  EXPECT_TRUE(sbx::reflection::visit_member(instance, "b", [](auto&) {}));
  EXPECT_TRUE(sbx::reflection::visit_member(instance, "c", [](auto&) {}));
}

TEST(reflection, iterates_enumerators) {
  auto visited = 0;

  sbx::reflection::for_each<test_enum>([&](auto name, auto value) {
    ++visited;
    (void)name;
    (void)value;
  });

  EXPECT_EQ(visited, 3);
  EXPECT_EQ(sbx::reflection::description<test_enum>::name(), "test_enum");
}

TEST(reflection, enum_to_string) {
  EXPECT_EQ(sbx::reflection::to_string(test_enum::one), "one");
  EXPECT_EQ(sbx::reflection::to_string(test_enum::two), "two");
  EXPECT_EQ(sbx::reflection::to_string(test_enum::three), "three");
}

TEST(reflection, enum_from_string) {
  const auto one = sbx::reflection::from_string<test_enum>("one");
  const auto two = sbx::reflection::from_string<test_enum>("two");
  const auto three = sbx::reflection::from_string<test_enum>("three");
  const auto bogus = sbx::reflection::from_string<test_enum>("bogus");

  ASSERT_TRUE(one.has_value());
  ASSERT_TRUE(two.has_value());
  ASSERT_TRUE(three.has_value());
  EXPECT_FALSE(bogus.has_value());

  EXPECT_EQ(*one, test_enum::one);
  EXPECT_EQ(*two, test_enum::two);
  EXPECT_EQ(*three, test_enum::three);
}

TEST(reflection, enum_round_trip) {
  for (const auto value : {test_enum::one, test_enum::two, test_enum::three}) {
    const auto name = sbx::reflection::to_string(value);
    const auto parsed = sbx::reflection::from_string<test_enum>(name);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, value);
  }
}
