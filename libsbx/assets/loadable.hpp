// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_LOADABLE_HPP_
#define LIBSBX_ASSETS_LOADABLE_HPP_

#include <cstdint>

#include <libsbx/signals/signal.hpp>

namespace sbx::assets {

/**
 * @brief Content readiness shared by every asset type, on the asset object so all handle copies see the same state.
 *
 * `generation()` starts at 0 and is bumped whenever real content replaces a placeholder or an update_* call applies an edit; `is_loaded()` is `generation() > 0`.
 */
class loadable {

public:

  [[nodiscard]] auto generation() const noexcept -> std::uint64_t {
    return _generation;
  }

  [[nodiscard]] auto is_loaded() const noexcept -> bool {
    return _generation > 0u;
  }

  template<typename Callable>
  auto on_loaded(Callable&& callable) -> void {
    _on_loaded.connect(std::forward<Callable>(callable));
  }

protected:

  auto _bump_generation() noexcept -> void {
    ++_generation;
    _on_loaded.emit(_generation);
  }

private:

  std::uint64_t _generation{0u};
  signals::signal<std::uint64_t> _on_loaded{};

}; // class loadable

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_LOADABLE_HPP_
