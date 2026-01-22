// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_RELATIONSHIP_HPP_
#define LIBSBX_SCENES_COMPONENTS_RELATIONSHIP_HPP_

#include <libsbx/math/uuid.hpp>

#include <vector>
#include <ranges>

#include <libsbx/scenes/node.hpp>

namespace sbx::scenes {

class relationship final {

public:

  relationship(const node parent)
  : _parent{parent} { }

  auto parent() const noexcept -> node {
    return _parent;
  }

  auto set_parent(const node parent) noexcept -> void {
    _parent = parent;
  }

  auto children() const noexcept -> const std::vector<node>& {
    return _children;
  }

  auto children() noexcept -> std::vector<node>& {
    return _children;
  }

  auto add_child(const node child) -> void {
    _children.push_back(child);
  }

  auto remove_child(const node child) -> void {
    std::erase(_children, child);
  }

private:

  node _parent;
  std::vector<node> _children;

}; // class relationship

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_RELATIONSHIP_HPP_
