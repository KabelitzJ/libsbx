// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <libsbx/ecs/registry.hpp>

class node {

  friend struct sbx::ecs::entity_traits<node>;

public:

  inline static constexpr auto null = sbx::ecs::null_entity;

  using entity_type = std::uint32_t;

  constexpr operator entity_type() const noexcept {
    return _value;
  }

  constexpr operator bool() const noexcept {
    return _value != null;
  }

private:

  constexpr node(const entity_type value) noexcept 
  : _value{value} { }

  entity_type _value;

}; // struct node

using registry_type = sbx::ecs::basic_registry<node>;

struct hierarchy {
  node parent{node::null};
  node first_child{node::null};
  node next_sibling{node::null};
  node previous_sibling{node::null};
}; // struct hierarchy

struct keep_tag { };
struct exclude_tag { };

TEST(libsbx_math_registry, initialize) {
  auto registry = registry_type{};

  auto e1 = registry.create();
  auto e2 = registry.create();
  auto e3 = registry.create();
  auto e4 = registry.create();

  registry.destroy(e4);

  e4 = registry.create();

  registry.emplace<keep_tag>(e1);

  registry.emplace<keep_tag>(e2);
  registry.emplace<exclude_tag>(e2);

  registry.emplace<keep_tag>(e3);

  registry.emplace<keep_tag>(e4);

  auto kept = 0u;

  // auto view = registry.view<keep_tag>();
  auto view = registry.view<keep_tag>(sbx::ecs::exclude<exclude_tag>);

  for (const auto node : view) {
    kept++;
  }

  EXPECT_EQ(kept, 3u);
}
