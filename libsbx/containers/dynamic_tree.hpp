// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/containers/dynamic_tree.hpp
 *
 * @brief A dynamic bounding-volume hierarchy (AABB tree) for moving objects — Box2D/Bullet-style:
 * a binary tree with fattened leaf AABBs, incremental insert/remove, refit-or-reinsert updates,
 * and stack-based AABB/ray queries. Unlike @ref octree (insert-only, unsuitable for moving
 * objects), this structure supports the full insert/remove/update lifecycle a physics broadphase
 * (or any other spatial index of moving objects — frustum culling, picking) needs.
 *
 * @ingroup libsbx-containers
 */

#ifndef LIBSBX_CONTAINERS_DYNAMIC_TREE_HPP_
#define LIBSBX_CONTAINERS_DYNAMIC_TREE_HPP_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/containers/static_vector.hpp>

namespace sbx::containers {

template<typename Type>
class dynamic_tree {

public:

  using id = std::uint32_t;

  inline static constexpr auto null = static_cast<id>(-1);

  /**
   * @brief One node's full internal state -- exposed read-only (see node_count()/node_at()) purely
   * as bulk structural access, the same kind of thing for_each_leaf()/query() already are. This
   * type has no notion of files or byte layout; a caller that wants to persist a tree (e.g. to skip
   * rebuilding it from scratch) owns that serialization itself, entirely outside this class -- see
   * rebuild() for the other half of that round trip.
   */
  struct node {
    math::volume aabb{};
    id parent{null};
    id child1{null};
    id child2{null};
    Type payload{};
    bool is_leaf{false};
    std::int32_t height{-1}; // -1 = on freelist
  }; // struct node

  dynamic_tree() noexcept = default;

  /**
   * @brief Inserts @p payload with tight bounds @p aabb (internally fattened by a fixed margin)
   * and returns a stable leaf id to later @ref update or @ref remove it.
   */
  [[nodiscard]] auto insert(const Type& payload, const math::volume& aabb) -> id {
    const auto leaf = _allocate_node();

    _nodes[leaf].aabb = _fatten(aabb);
    _nodes[leaf].payload = payload;
    _nodes[leaf].is_leaf = true;
    _nodes[leaf].height = 0;

    _insert_leaf(leaf);

    return leaf;
  }

  /**
   * @brief Removes @p leaf. Invalidates that id.
   */
  auto remove(id leaf) -> void {
    _remove_leaf(leaf);
    _free_node(leaf);
  }

  /**
   * @brief Refits @p leaf for a new tight AABB. If the existing fattened AABB still contains
   * @p tight_aabb this is a no-op (returns false) — the common case for slow-moving or stationary
   * bodies. Otherwise removes and reinserts the leaf, reusing the same id (returns true).
   */
  [[nodiscard]] auto update(id leaf, const math::volume& tight_aabb) -> bool {
    if (_nodes[leaf].aabb.contains(tight_aabb)) {
      return false;
    }

    _remove_leaf(leaf);

    _nodes[leaf].aabb = _fatten(tight_aabb);

    _insert_leaf(leaf);

    return true;
  }

  auto clear() -> void {
    _nodes.clear();
    _root = null;
    _free_list = null;
  }

  [[nodiscard]] auto is_empty() const noexcept -> bool {
    return _root == null;
  }

  [[nodiscard]] auto aabb_of(id leaf) const -> const math::volume& {
    return _nodes[leaf].aabb;
  }

  [[nodiscard]] auto payload_of(id leaf) const -> const Type& {
    return _nodes[leaf].payload;
  }

  /**
   * @brief Invokes callback(const Type&) for every leaf whose (fattened) AABB overlaps @p aabb.
   */
  template<typename Fn>
  auto query(const math::volume& aabb, Fn&& callback) const -> void {
    if (_root == null) {
      return;
    }

    auto stack = static_vector<id, 256u>{};
    stack.push_back(_root);

    while (!stack.is_empty()) {
      const auto current = stack.back();
      stack.pop_back();

      const auto& node = _nodes[current];

      if (!node.aabb.intersects(aabb)) {
        continue;
      }

      if (node.is_leaf) {
        std::invoke(callback, node.payload);
      } else {
        stack.push_back(node.child1);
        stack.push_back(node.child2);
      }
    }
  }

  /**
   * @brief Invokes callback(const Type&, std::float_t hit_t) for every leaf whose (fattened) AABB
   * the ray intersects, in no particular order — callers wanting the nearest hit must reduce
   * themselves.
   */
  template<typename Fn>
  auto query(const math::ray& ray, Fn&& callback) const -> void {
    if (_root == null) {
      return;
    }

    auto stack = static_vector<id, 256u>{};
    stack.push_back(_root);

    while (!stack.is_empty()) {
      const auto current = stack.back();
      stack.pop_back();

      const auto& node = _nodes[current];

      const auto hit = node.aabb.intersects(ray);

      if (!hit) {
        continue;
      }

      if (node.is_leaf) {
        std::invoke(callback, node.payload, *hit);
      } else {
        stack.push_back(node.child1);
        stack.push_back(node.child2);
      }
    }
  }

  /**
   * @brief Invokes callback(id, const Type&, const math::volume& fat_aabb) for every leaf.
   */
  template<typename Fn>
  auto for_each_leaf(Fn&& callback) const -> void {
    for (auto index = id{0u}; index < static_cast<id>(_nodes.size()); ++index) {
      if (_nodes[index].height >= 0 && _nodes[index].is_leaf) {
        std::invoke(callback, index, _nodes[index].payload, _nodes[index].aabb);
      }
    }
  }

  /** @brief Number of node slots, including any currently on the free list -- node_at(i) is valid for every i < node_count(). */
  [[nodiscard]] auto node_count() const noexcept -> std::size_t {
    return _nodes.size();
  }

  /** @brief Read-only access to one node slot by its raw index (not necessarily a leaf, and not necessarily live -- height == -1 means it's on the free list). See node_count()/root_id() for bulk-exporting the whole tree. */
  [[nodiscard]] auto node_at(id index) const -> const node& {
    return _nodes[index];
  }

  [[nodiscard]] auto root_id() const noexcept -> id {
    return _root;
  }

  /**
   * @brief Replaces this tree's entire contents with a previously-exported node array (see
   * node_count()/node_at()/root_id()) and its root, resetting the free list empty. For rebuilding a
   * tree that was insert-only when exported (remove() never called on it) directly from stored
   * data -- e.g. a disk-cached BVH -- skipping normal incremental insert()-based construction
   * entirely. `nodes` must already be in this exact internal layout (parent/child links, heights,
   * fattened AABBs and all) -- this does no validation or rebalancing of its own.
   */
  auto rebuild(std::vector<node> nodes, id root) -> void {
    _nodes = std::move(nodes);
    _root = root;
    _free_list = null;
  }

private:

  inline static constexpr auto fatten_margin = std::float_t{0.1f};

  [[nodiscard]] static auto _fatten(const math::volume& aabb) -> math::volume {
    const auto margin = math::vector3{fatten_margin, fatten_margin, fatten_margin};
    return math::volume{aabb.min() - margin, aabb.max() + margin};
  }

  [[nodiscard]] static auto _surface_area(const math::volume& aabb) noexcept -> std::float_t {
    const auto extent = aabb.extend();
    return 2.0f * (extent.x() * extent.y() + extent.y() * extent.z() + extent.z() * extent.x());
  }

  [[nodiscard]] auto _allocate_node() -> id {
    if (_free_list != null) {
      const auto allocated = _free_list;
      _free_list = _nodes[allocated].parent; // freelist is threaded through `parent`
      _nodes[allocated] = node{};
      return allocated;
    }

    _nodes.emplace_back();
    return static_cast<id>(_nodes.size() - 1u);
  }

  auto _free_node(id index) -> void {
    _nodes[index].height = -1;
    _nodes[index].parent = _free_list;
    _free_list = index;
  }

  auto _insert_leaf(id leaf) -> void {
    if (_root == null) {
      _root = leaf;
      _nodes[leaf].parent = null;
      return;
    }

    const auto leaf_aabb = _nodes[leaf].aabb;

    // Descend from the root, at each step choosing the child that minimizes the surface area of
    // the merged AABB the new leaf would create alongside it (Box2D's insertion heuristic).
    auto index = _root;

    while (!_nodes[index].is_leaf) {
      const auto child1 = _nodes[index].child1;
      const auto child2 = _nodes[index].child2;

      const auto area = _surface_area(_nodes[index].aabb);
      const auto combined_area = _surface_area(math::volume::merge(_nodes[index].aabb, leaf_aabb));

      const auto inheritance_cost = 2.0f * (combined_area - area);

      const auto cost_of = [&](id child) -> std::float_t {
        const auto merged = _surface_area(math::volume::merge(leaf_aabb, _nodes[child].aabb));

        if (_nodes[child].is_leaf) {
          return merged + inheritance_cost;
        }

        return (merged - _surface_area(_nodes[child].aabb)) + inheritance_cost;
      };

      const auto cost1 = cost_of(child1);
      const auto cost2 = cost_of(child2);

      if (combined_area * 2.0f < cost1 && combined_area * 2.0f < cost2) {
        break;
      }

      index = (cost1 < cost2) ? child1 : child2;
    }

    const auto sibling = index;
    const auto old_parent = _nodes[sibling].parent;

    const auto new_parent = _allocate_node();

    _nodes[new_parent].parent = old_parent;
    _nodes[new_parent].aabb = math::volume::merge(leaf_aabb, _nodes[sibling].aabb);
    _nodes[new_parent].height = _nodes[sibling].height + 1;
    _nodes[new_parent].is_leaf = false;
    _nodes[new_parent].child1 = sibling;
    _nodes[new_parent].child2 = leaf;

    _nodes[sibling].parent = new_parent;
    _nodes[leaf].parent = new_parent;

    if (old_parent == null) {
      _root = new_parent;
    } else {
      if (_nodes[old_parent].child1 == sibling) {
        _nodes[old_parent].child1 = new_parent;
      } else {
        _nodes[old_parent].child2 = new_parent;
      }
    }

    // Walk back up to the root, refitting bounds/heights and rebalancing.
    index = _nodes[leaf].parent;

    while (index != null) {
      index = _balance(index);

      const auto child1 = _nodes[index].child1;
      const auto child2 = _nodes[index].child2;

      _nodes[index].height = 1 + std::max(_nodes[child1].height, _nodes[child2].height);
      _nodes[index].aabb = math::volume::merge(_nodes[child1].aabb, _nodes[child2].aabb);

      index = _nodes[index].parent;
    }
  }

  auto _remove_leaf(id leaf) -> void {
    if (leaf == _root) {
      _root = null;
      return;
    }

    const auto parent = _nodes[leaf].parent;
    const auto grandparent = _nodes[parent].parent;
    const auto sibling = (_nodes[parent].child1 == leaf) ? _nodes[parent].child2 : _nodes[parent].child1;

    if (grandparent == null) {
      _root = sibling;
      _nodes[sibling].parent = null;
      _free_node(parent);
      return;
    }

    if (_nodes[grandparent].child1 == parent) {
      _nodes[grandparent].child1 = sibling;
    } else {
      _nodes[grandparent].child2 = sibling;
    }

    _nodes[sibling].parent = grandparent;
    _free_node(parent);

    auto index = grandparent;

    while (index != null) {
      index = _balance(index);

      const auto child1 = _nodes[index].child1;
      const auto child2 = _nodes[index].child2;

      _nodes[index].height = 1 + std::max(_nodes[child1].height, _nodes[child2].height);
      _nodes[index].aabb = math::volume::merge(_nodes[child1].aabb, _nodes[child2].aabb);

      index = _nodes[index].parent;
    }
  }

  /**
   * @brief Classic AVL-style single/double rotation around @p node_id if its two subtrees'
   * heights differ by more than one. Direct port of the well-known Box2D b2DynamicTree::Balance
   * algorithm (two symmetric cases: the right subtree too tall, or the left). Returns the
   * (possibly new) root of this subtree.
   */
  [[nodiscard]] auto _balance(id node_id) -> id {
    if (_nodes[node_id].is_leaf || _nodes[node_id].height < 2) {
      return node_id;
    }

    const auto left = _nodes[node_id].child1;
    const auto right = _nodes[node_id].child2;

    const auto balance = _nodes[right].height - _nodes[left].height;

    // Right subtree too tall: rotate it up.
    if (balance > 1) {
      const auto grandchild1 = _nodes[right].child1;
      const auto grandchild2 = _nodes[right].child2;

      // Swap node_id and right.
      _nodes[right].child1 = node_id;
      _nodes[right].parent = _nodes[node_id].parent;
      _nodes[node_id].parent = right;

      if (_nodes[right].parent != null) {
        if (_nodes[_nodes[right].parent].child1 == node_id) {
          _nodes[_nodes[right].parent].child1 = right;
        } else {
          _nodes[_nodes[right].parent].child2 = right;
        }
      } else {
        _root = right;
      }

      if (_nodes[grandchild1].height > _nodes[grandchild2].height) {
        _nodes[right].child2 = grandchild1;
        _nodes[node_id].child2 = grandchild2;
        _nodes[grandchild2].parent = node_id;
        _nodes[node_id].aabb = math::volume::merge(_nodes[left].aabb, _nodes[grandchild2].aabb);
        _nodes[right].aabb = math::volume::merge(_nodes[node_id].aabb, _nodes[grandchild1].aabb);
        _nodes[node_id].height = 1 + std::max(_nodes[left].height, _nodes[grandchild2].height);
        _nodes[right].height = 1 + std::max(_nodes[node_id].height, _nodes[grandchild1].height);
      } else {
        _nodes[right].child2 = grandchild2;
        _nodes[node_id].child2 = grandchild1;
        _nodes[grandchild1].parent = node_id;
        _nodes[node_id].aabb = math::volume::merge(_nodes[left].aabb, _nodes[grandchild1].aabb);
        _nodes[right].aabb = math::volume::merge(_nodes[node_id].aabb, _nodes[grandchild2].aabb);
        _nodes[node_id].height = 1 + std::max(_nodes[left].height, _nodes[grandchild1].height);
        _nodes[right].height = 1 + std::max(_nodes[node_id].height, _nodes[grandchild2].height);
      }

      return right;
    }

    // Left subtree too tall: rotate it up (mirror of the above).
    if (balance < -1) {
      const auto grandchild1 = _nodes[left].child1;
      const auto grandchild2 = _nodes[left].child2;

      // Swap node_id and left.
      _nodes[left].child1 = node_id;
      _nodes[left].parent = _nodes[node_id].parent;
      _nodes[node_id].parent = left;

      if (_nodes[left].parent != null) {
        if (_nodes[_nodes[left].parent].child1 == node_id) {
          _nodes[_nodes[left].parent].child1 = left;
        } else {
          _nodes[_nodes[left].parent].child2 = left;
        }
      } else {
        _root = left;
      }

      if (_nodes[grandchild1].height > _nodes[grandchild2].height) {
        _nodes[left].child2 = grandchild1;
        _nodes[node_id].child1 = grandchild2;
        _nodes[grandchild2].parent = node_id;
        _nodes[node_id].aabb = math::volume::merge(_nodes[right].aabb, _nodes[grandchild2].aabb);
        _nodes[left].aabb = math::volume::merge(_nodes[node_id].aabb, _nodes[grandchild1].aabb);
        _nodes[node_id].height = 1 + std::max(_nodes[right].height, _nodes[grandchild2].height);
        _nodes[left].height = 1 + std::max(_nodes[node_id].height, _nodes[grandchild1].height);
      } else {
        _nodes[left].child2 = grandchild2;
        _nodes[node_id].child1 = grandchild1;
        _nodes[grandchild1].parent = node_id;
        _nodes[node_id].aabb = math::volume::merge(_nodes[right].aabb, _nodes[grandchild1].aabb);
        _nodes[left].aabb = math::volume::merge(_nodes[node_id].aabb, _nodes[grandchild2].aabb);
        _nodes[node_id].height = 1 + std::max(_nodes[right].height, _nodes[grandchild1].height);
        _nodes[left].height = 1 + std::max(_nodes[node_id].height, _nodes[grandchild2].height);
      }

      return left;
    }

    return node_id;
  }

  std::vector<node> _nodes{};
  id _root{null};
  id _free_list{null};

}; // class dynamic_tree

} // namespace sbx::containers

#endif // LIBSBX_CONTAINERS_DYNAMIC_TREE_HPP_
