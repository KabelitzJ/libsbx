// SPDX-License-Identifier: MIT
/**
 * @file libsbx/ecs/sparse_set.hpp
 * 
 * @brief Sparse set container for ECS entity storage.
 *
 * @details
 * 
 * This header defines a sparse set data structure optimized for use in
 * entity-component systems. The container maintains a dense array of entities
 * and a sparse mapping that allows constant-time lookup, insertion, and
 * removal.
 *
 * Multiple deletion strategies are supported via configurable policies,
 * enabling trade-offs between memory locality, iteration stability, and
 * deletion cost.
 *
 * The sparse set is allocator-aware and designed to be extensible through
 * inheritance.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::ecs
 * @version 0.1.0
 * @since 2024-01-06
 */

#ifndef LIBSBX_SPARSE_SET_HPP_
#define LIBSBX_SPARSE_SET_HPP_

#include <memory>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <functional>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/algorithm.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/memory/concepts.hpp>

#include <libsbx/ecs/detail/sparse_set_iterator.hpp>

namespace sbx::ecs {

/**
 * @brief Deletion behavior policy for sparse sets.
 */
enum class deletion_policy : std::uint8_t {
  swap_and_pop = 0u,
  in_place = 1u,
  swap_only = 2u,
  unspecified = swap_and_pop
}; // enum class deletion_policy

/**
 * @brief Sparse set container for entity identifiers.
 *
 * @tparam Entity    Entity identifier type.
 * @tparam Allocator Allocator type.
 */
template<typename Entity, memory::allocator_for<Entity> Allocator = std::allocator<Entity>>
class basic_sparse_set {

  using allocator_traits = std::allocator_traits<Allocator>;
  using entity_traits = ecs::entity_traits<Entity>;

  using dense_storage_type = std::vector<Entity, Allocator>;
  using sparse_storage_type = std::vector<typename allocator_traits::pointer, typename allocator_traits::rebind_alloc<typename allocator_traits::pointer>>;

  inline static constexpr auto max_size = static_cast<std::size_t>(entity_traits::to_entity(null_entity));

public:

  using allocator_type = Allocator;
  using entity_type = entity_traits::value_type;
  using version_type = entity_traits::version_type;
  using pointer = typename dense_storage_type::pointer;
  using const_pointer = typename dense_storage_type::const_pointer;
  using reference = typename dense_storage_type::reference;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = detail::sparse_set_iterator<dense_storage_type>;
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /**
   * @brief Constructs an empty sparse set.
   *
   * @param policy    Deletion policy.
   * @param allocator Allocator instance.
   */
  basic_sparse_set(const deletion_policy policy = deletion_policy::unspecified, const allocator_type& allocator = allocator_type{});

    /**
   * @brief Deleted copy constructor.
   *
   * @details
   *
   * Copying a sparse set is explicitly disallowed to prevent accidental
   * duplication of internal sparse and dense storage.
   * 
   * @param other Source sparse set.
   */
  basic_sparse_set(const basic_sparse_set& other) = delete;

  /**
   * @brief Move-constructs a sparse set.
   *
   * @details
   *
   * Transfers ownership of the internal storage from the source instance.
   * The moved-from sparse set is left in a valid but unspecified state.
   *
   * @param other Source sparse set.
   */
  basic_sparse_set(basic_sparse_set&& other) noexcept;

  /**
   * @brief Move-constructs a sparse set using a specific allocator.
   *
   * @details
   *
   * Moves the contents of the source sparse set while rebinding storage to
   * the provided allocator. If allocator propagation is not permitted, the
   * allocators must compare equal.
   *
   * @param other     Source sparse set.
   * @param allocator Allocator to use for the new instance.
   */
  basic_sparse_set(basic_sparse_set&& other, const allocator_type& allocator);

  /**
   * @brief Destroys the sparse set.
   *
   * @details
   *
   * Releases all allocated sparse pages and clears internal storage.
   */
  virtual ~basic_sparse_set();

  /**
   * @brief Deleted copy assignment operator.
   *
   * @details
   *
   * Copy assignment is explicitly disallowed to avoid unsafe duplication
   * of sparse mappings and dense storage.
   *
   * @param other Source sparse set.
   *
   * @return Reference to this sparse set.
   */
  auto operator=(const basic_sparse_set& other) -> basic_sparse_set& = delete;

  /**
   * @brief Move-assigns a sparse set.
   *
   * @details
   *
   * Transfers ownership of all internal storage from the source instance.
   * Allocators must be compatible or always equal.
   *
   * @param other Source sparse set.
   *
   * @return Reference to this sparse set.
   */
  auto operator=(basic_sparse_set&& other) noexcept -> basic_sparse_set&;

  /**
   * @brief Swaps the contents of two sparse sets.
   */
  auto swap(basic_sparse_set& other) noexcept -> void;

  /**
   * @brief Returns the allocator associated with the container.
   */
  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type;

  /**
   * @brief Returns the deletion policy.
   */
  [[nodiscard]] auto policy() const noexcept -> deletion_policy;

  /**
   * @brief Returns the index of the free-list head.
   */
  [[nodiscard]] auto free_list() const noexcept -> size_type;

  /**
   * @brief Reserves dense storage capacity.
   */
  virtual void reserve(const size_type capacity);

  /**
   * @brief Returns dense storage capacity.
   */
  [[nodiscard]] virtual auto capacity() const noexcept -> size_type;

  /**
   * @brief Returns sparse storage extent.
   */
  [[nodiscard]] auto extent() const noexcept -> size_type;

  /**
   * @brief Returns the number of stored entities.
   */
  [[nodiscard]] auto size() const noexcept -> size_type;

  /**
   * @brief Checks whether the set is empty.
   */
  [[nodiscard]] auto is_empty() const noexcept -> bool;

  /**
   * @brief Checks whether the dense storage is contiguous.
   */
  [[nodiscard]] auto is_contiguous() const noexcept -> bool;

  /**
   * @brief Returns pointer to dense storage.
   */
  [[nodiscard]] auto data() const noexcept -> const_pointer;

  /**
   * @brief Returns pointer to dense storage.
   */
  [[nodiscard]] auto data() noexcept -> pointer;

  /**
   * @brief Updates the version of an entity.
   */
  auto bump(const entity_type entity) -> version_type;

  /**
   * @brief Returns iterator to the first element.
   */
  [[nodiscard]] auto begin() const noexcept -> iterator;

  /**
   * @brief Returns const iterator to the first element.
   */
  [[nodiscard]] auto cbegin() const noexcept -> const_iterator;

  /**
   * @brief Returns iterator to the end.
   */
  [[nodiscard]] auto end() const noexcept -> iterator;

  /**
   * @brief Returns const iterator to the end.
   */
  [[nodiscard]] auto cend() const noexcept -> const_iterator;

  /**
   * @brief Checks whether an entity exists in the set.
   */
  [[nodiscard]] bool contains(const entity_type entity) const noexcept;

  /**
   * @brief Returns the current version of an entity.
   */
  [[nodiscard]] auto current(const entity_type entity) const noexcept -> version_type;

  /**
   * @brief Finds an entity iterator.
   */
  [[nodiscard]] auto find(const entity_type entity) const noexcept -> const_iterator;

  /**
   * @brief Returns the dense index of an entity.
   */
  [[nodiscard]] auto index(const entity_type entity) const noexcept -> size_type;

  /**
   * @brief Removes an entity.
   */
  auto erase(const entity_type entity) -> void;

  /**
   * @brief Removes an entity if present.
   */
  auto remove(const entity_type entity) -> bool;

  /**
   * @brief Sorts entities using a comparator.
   */
  template<typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort(Compare compare, Sort sort = Sort{}, Args&&... args) -> void;

  /**
   * @brief Sorts the first N entities.
   */
  template<typename Compare, typename Sort = utility::std_sort, typename... Args>
  auto sort_n(const size_type length, Compare compare, Sort sort = Sort{}, Args&&... args) -> void;

  /**
   * @brief Clears all entities.
   */
  auto clear() -> void;

  /**
   * @brief Invokes a tagged callback on an entity.
   */
  auto invoke(const utility::hashed_string& tag, const entity_type entity) -> void;

protected:

  using basic_iterator = iterator;

  /**
   * @brief Virtual callback hook.
   */
  virtual auto call(const utility::hashed_string& tag, const entity_type entity) -> void;

  void swap_only(const basic_iterator iterator);

  auto swap_and_pop(const basic_iterator iterator) -> void;

  auto in_place_pop(const basic_iterator iterator) -> void;

  virtual auto pop(basic_iterator first, basic_iterator last) -> void;

  virtual auto pop_all() -> void;

  virtual auto try_emplace(const entity_type entity, const bool force_back) -> basic_iterator;

private:

  [[nodiscard]] virtual auto get_at(const std::size_t) const -> const void*;

  virtual auto _swap_or_move(const std::size_t from, const std::size_t to) -> void;

  [[nodiscard]] auto _policy_to_head() const noexcept -> size_type;

  [[nodiscard]] auto _entity_to_position(const entity_type entity) const noexcept;

  [[nodiscard]] auto _position_to_page(const size_type position) const noexcept;

  [[nodiscard]] auto _sparse_pointer(const entity_type entity) const -> pointer;

  [[nodiscard]] auto _sparse_reference(const entity_type entity) const -> reference;

  void _release_sparse_pages();

  [[nodiscard]] auto _to_iterator(const entity_type entity) const noexcept -> iterator;

  [[nodiscard]] auto _assure_at_least(const entity_type entity) -> reference;

  auto _swap_at(const size_type lhs, const size_type rhs) -> void;

  dense_storage_type _dense;
  sparse_storage_type _sparse;
  deletion_policy _policy;
  size_type _head;

}; // class basic_sparse_set

} // namespace sbx::ecs

#include <libsbx/ecs/sparse_set.ipp>

#endif // LIBSBX_SPARSE_SET_HPP_
