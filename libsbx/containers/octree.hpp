// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CONTAINERS_OCTREE_HPP_
#define LIBSBX_CONTAINERS_OCTREE_HPP_

#include <range/v3/all.hpp>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/memory/concepts.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/box.hpp>

#include <libsbx/containers/static_vector.hpp>

namespace sbx::containers {

template<typename Type, std::size_t Threshold = 16u, std::size_t Depth = 8u>
class octree {

  inline static constexpr auto threshold = Threshold;
  inline static constexpr auto depth = Depth;

  struct node {

    using id = std::uint32_t;

    inline static constexpr auto children_count = std::size_t{8u};

    struct value_type {
      Type value;
      math::volume bounds;
    }; // struct value_type

    inline constexpr static auto null = static_cast<id>(-1);

    node()
    : values{utility::make_reserved_vector<value_type>(threshold)},
      children{null, null, null, null, null, null, null, null} { }

    auto is_leaf() const noexcept -> bool {
      return children[0u] == null;
    }

    auto child_at(const std::size_t index) const noexcept -> id {
      return children[index];
    }

    auto push_back(const value_type& value) noexcept -> void {
      values.push_back(value);
    }

    auto push_back(value_type&& value) noexcept -> void {
      values.push_back(std::move(value));
    }

    static auto find_volume(const math::volume& outer, const math::volume& inner) noexcept -> std::optional<std::uint32_t> {
      auto center = outer.center();

      if (!outer.contains(inner)) {
        return std::nullopt;
      }

      if (inner.max().x() <= center.x() && inner.max().y() <= center.y() && inner.max().z() <= center.z()) {
        return 0u;
      }

      if (inner.min().x() >= center.x() && inner.max().y() <= center.y() && inner.max().z() <= center.z()) {
        return 1u;
      }

      if (inner.max().x() <= center.x() && inner.min().y() >= center.y() && inner.max().z() <= center.z()) {
        return 2u;
      }

      if (inner.min().x() >= center.x() && inner.min().y() >= center.y() && inner.max().z() <= center.z()) {
        return 3u;
      }

      if (inner.max().x() <= center.x() && inner.max().y() <= center.y() && inner.min().z() >= center.z()) {
        return 4u;
      }

      if (inner.min().x() >= center.x() && inner.max().y() <= center.y() && inner.min().z() >= center.z()) {
        return 5u;
      }

      if (inner.max().x() <= center.x() && inner.min().y() >= center.y() && inner.min().z() >= center.z()) {
        return 6u;
      }

      if (inner.min().x() >= center.x() && inner.min().y() >= center.y() && inner.min().z() >= center.z()) {
        return 7u;
      }

      return std::nullopt;
    }

    static auto child_bounds(const math::volume& outer, const std::uint32_t index) -> math::volume {
      const auto min = outer.min();
      const auto max = outer.max();
      const auto center = (min + max) * 0.5f;

      switch (index) {
        case 0: return math::volume{min, center};
        case 1: return math::volume{math::vector3{center.x(), min.y(), min.z()}, math::vector3{max.x(), center.y(), center.z()}};
        case 2: return math::volume{math::vector3{min.x(), center.y(), min.z()}, math::vector3{center.x(), max.y(), center.z()}};
        case 3: return math::volume{math::vector3{center.x(), center.y(), min.z()}, math::vector3{max.x(), max.y(), center.z()}};
        case 4: return math::volume{math::vector3{min.x(), min.y(), center.z()}, math::vector3{center.x(), center.y(), max.z()}};
        case 5: return math::volume{math::vector3{center.x(), min.y(), center.z()}, math::vector3{max.x(), center.y(), max.z()}};
        case 6: return math::volume{math::vector3{min.x(), center.y(), center.z()}, math::vector3{center.x(), max.y(), max.z()}};
        case 7: return math::volume{center, max};
      }

      throw std::runtime_error("Invalid index");
    }

    std::vector<value_type> values;
    std::array<id, children_count> children;
  
  }; // struct node

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;

  struct intersection {
    value_type first;
    value_type second;
  }; // struct intersection

  struct inside_result {
    value_type value;
    const math::volume& bounds;
  }; // struct inside_result

  octree(const math::volume& bounds, const std::size_t estimated_elements) noexcept
  : _bounds{bounds},
    _root{0u} {
    _nodes.reserve(estimated_elements / 2);
    _nodes.push_back(node{});
  }

  ~octree() {

  }

  auto insert(const value_type& value, const math::volume& bounds) noexcept -> void {
    _insert(_root, _bounds, value, bounds, 0u);
  }

  auto intersections() -> std::vector<intersection> {
    auto intersections = utility::make_reserved_vector<intersection>(_nodes.size());

    _intersections(_root, _bounds, intersections);

    intersections.shrink_to_fit();

    return intersections;
  }

  auto inside(const math::box& box) -> std::vector<inside_result> {
    auto inside = std::vector<inside_result>{};

    _inside(_root, _bounds, box, inside);

    return inside;
  }

  auto clear() -> void {
    _nodes.resize(1); 

    _nodes[0].values.clear();
    _nodes[0].children.fill(node::null);
  }

  template<typename Fn>
  requires (std::is_invocable_v<Fn, const math::volume&>)
  auto for_each_volume(Fn&& fn) -> void {
    _for_each_volume(_root, _bounds, std::forward<Fn>(fn));
  }

private:

  auto _insert(const node::id node_id, const math::volume& bounds, const value_type& value, const math::volume& value_bounds, const std::size_t current_depth) noexcept -> void {
    if (!bounds.contains(value_bounds)) {
      return;
    }

    if (_nodes[node_id].is_leaf()) {
      if (_nodes[node_id].values.size() < threshold || current_depth >= depth) {
        _nodes[node_id].push_back({value, value_bounds});
      } else {
        _split(node_id, bounds);
        _insert(node_id, bounds, value, value_bounds, current_depth);
      }
    } else {
      const auto quadrant = node::find_volume(bounds, value_bounds);

      if (quadrant) {
        _insert(_nodes[node_id].child_at(*quadrant), node::child_bounds(bounds, *quadrant), value, value_bounds, current_depth + 1u);
      } else {
        _nodes[node_id].values.push_back({value, value_bounds});
      }
    }
  }

  auto _split(node::id node_id, const math::volume& bounds) -> void {
    const auto first_child_id = static_cast<node::id>(_nodes.size());
    
    _nodes.resize(_nodes.size() + node::children_count);

    for (auto i = 0u; i < node::children_count; ++i) {
      _nodes[node_id].children[i] = first_child_id + i;
    }

    auto current_node_values = std::move(_nodes[node_id].values);

    _nodes[node_id].values.clear();

    for (auto&& value : current_node_values) {
      const auto quadrant = node::find_volume(bounds, value.bounds);

      if (quadrant) {
        _nodes[first_child_id + *quadrant].values.push_back(std::move(value));
      } else {
        _nodes[node_id].values.push_back(std::move(value));
      }
    }
  }

  auto _intersections(node::id node_id, const math::volume& bounds, std::vector<intersection>& intersections) -> void {
    for (auto i : std::views::iota(0u, _nodes[node_id].values.size())) {
      for (auto j : std::views::iota(0u, i)) {
        if (_nodes[node_id].values[i].bounds.intersects(_nodes[node_id].values[j].bounds)) {
          intersections.push_back({_nodes[node_id].values[i].value, _nodes[node_id].values[j].value});
        }
      }
    }

    if (!_nodes[node_id].is_leaf()) {
      for (const auto& child_id : _nodes[node_id].children) {
        for (const auto& [value, bounds] : _nodes[node_id].values) {
          _intersections_with_descendants(child_id, value, bounds, intersections);
        }
      }

      for (auto i : std::views::iota(0u, _nodes[node_id].children.size())) {
        auto child_bounds = node::child_bounds(bounds, i);

        _intersections(_nodes[node_id].child_at(i), child_bounds, intersections);
      }
    }
  }

  auto _intersections_with_descendants(node::id node_id, const value_type& value, const math::volume& value_bounds, std::vector<intersection>& intersections) -> void {
    for (const auto& [node_value, bound] :  _nodes[node_id].values) {
      if (bound.intersects(value_bounds)) {
        intersections.push_back({value, node_value});
      }
    }

    if (!_nodes[node_id].is_leaf()) {
      for (const auto& child : _nodes[node_id].children) {
        _intersections_with_descendants(child, value, value_bounds, intersections);
      }
    }
  }

  template<typename Fn>
  auto _for_each_volume(node::id node_id, const math::volume& bounds, Fn&& fn) -> void {
    std::invoke(fn, bounds);

    if (!_nodes[node_id].is_leaf()) {
      for (auto i = 0u; i < _nodes[node_id].children.size(); ++i) {
        const auto child_id = _nodes[node_id].child_at(i);
        const auto child_volume = node::child_bounds(bounds, i);
        
        _for_each_volume(child_id, child_volume, std::forward<Fn>(fn));
      }
    }
  }

  auto _inside(node::id node_id, const math::volume& bounds, const math::box& box, std::vector<inside_result>& inside) -> void {
    // Early-out if node's bounding volume doesn't intersect the box
    if (!box.intersects(bounds)) {
      return;
    }

    // Check all values in this node
    for (const auto& [value, value_bounds] : _nodes[node_id].values) {
      if (box.intersects(value_bounds)) {
        inside.push_back({value, value_bounds});
      }
    }

    // Recurse into children if not a leaf
    if (!_nodes[node_id].is_leaf()) {
      for (auto i : std::views::iota(0u, _nodes[node_id].children.size())) {
        auto child_id = _nodes[node_id].child_at(i);
        auto child_bounds = node::child_bounds(bounds, i);

        _inside(child_id, child_bounds, box, inside);
      }
    }
  }

  math::volume _bounds;
  node::id _root;
  std::vector<node> _nodes;

}; // class octree

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_OCTREE_HPP_
