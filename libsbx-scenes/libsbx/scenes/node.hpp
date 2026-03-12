// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_NODE_HPP_
#define LIBSBX_SCENES_NODE_HPP_

#include <unordered_set>

#include <libsbx/ecs/registry.hpp>
#include <libsbx/ecs/entity.hpp>

namespace sbx::scenes {

enum class node : std::uint32_t {
  null = ecs::null_entity
}; // enum class node

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_NODE_HPP_
