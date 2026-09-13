// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSET_HANDLE_HPP_
#define LIBSBX_ASSETS_ASSET_HANDLE_HPP_

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A ref-counted handle to a loaded asset.
 *
 * Holding one keeps the asset alive.
 *
 * is_loaded()/generation() are constrained per-method (`requires std::derived_from<value_type,
 * loadable>`), not on the class template itself, even though every current asset type is loadable:
 * a class-level constraint needs Type complete wherever asset_handle<Type> is first named, and
 * particle_effect.hpp's sub_emitter_binding names asset_handle<particle_effect> while
 * particle_effect is still an incomplete forward declaration (a particle_effect's own sub-emitters
 * can reference another particle_effect) -- a class-level `derived_from` check fails right there
 * with "invalid use of incomplete type". A per-method constraint is only evaluated when is_loaded()/
 * generation() are actually called, by which point every real caller has a complete type.
 */
template<typename Type>
class asset_handle {

public:

  using value_type = std::remove_cvref_t<Type>;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  asset_handle() = default;

  explicit asset_handle(std::shared_ptr<value_type> record)
  : _record{std::move(record)} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _record != nullptr;
  }

  [[nodiscard]] operator bool() const noexcept {
    return is_valid();
  }

  /** @brief Whether the asset's content (not just this handle) has arrived -- see loadable's doc comment. False for an invalid handle. */
  [[nodiscard]] auto is_loaded() const noexcept -> bool requires (std::derived_from<value_type, loadable>) {
    return is_valid() && _record->is_loaded();
  }

  /** @brief See loadable's doc comment. 0 for an invalid handle. */
  [[nodiscard]] auto generation() const noexcept -> std::uint64_t requires (std::derived_from<value_type, loadable>) {
    return is_valid() ? _record->generation() : 0u;
  }

  template<typename Callable>
  auto on_loaded(Callable&& callable) -> void requires (std::derived_from<value_type, loadable>) {
    _record->on_loaded(std::forward<Callable>(callable));
  }

  [[nodiscard]] auto operator->() const noexcept -> const_pointer {
    return _record.get();
  }

  [[nodiscard]] auto operator->() noexcept -> pointer {
    return _record.get();
  }

  [[nodiscard]] auto operator*() const noexcept -> const_reference {
    return *_record;
  }

  [[nodiscard]] auto operator*() noexcept -> reference {
    return *_record;
  }

  [[nodiscard]] auto get() const noexcept -> const_pointer {
    return _record.get();
  }

  [[nodiscard]] auto get() noexcept -> pointer {
    return _record.get();
  }

  /** @brief The underlying shared_ptr itself, e.g. to re-insert this handle's record into a uuid-keyed cache map that stores shared_ptr<Type> directly rather than a handle. */
  [[nodiscard]] auto shared() const noexcept -> std::shared_ptr<value_type> {
    return _record;
  }

private:

  std::shared_ptr<value_type> _record{};

}; // class asset_handle

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSET_HANDLE_HPP_
