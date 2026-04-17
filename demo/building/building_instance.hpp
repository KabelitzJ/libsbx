// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDINGS_BUILDING_INSTANCE_HPP_
#define DEMO_BUILDINGS_BUILDING_INSTANCE_HPP_

#include <cstdint>

#include <libsbx/scenes/scenes.hpp>

#include <demo/building/building_definition.hpp>

namespace demo {

enum class building_state : std::uint8_t {
  planned,
  under_construction,
  operational,
  degraded,
  abandoned
}; // enum class building_state

struct building_instance {
  std::uint32_t id{};
  std::uint32_t definition_id{};
  std::int32_t origin_x{};
  std::int32_t origin_z{};
  orientation orient{orientation::n};
  building_state state{building_state::planned};
  sbx::scenes::node scene_node{};
}; // struct building_instance

} // namespace demo

#endif // DEMO_BUILDINGS_BUILDING_INSTANCE_HPP_
