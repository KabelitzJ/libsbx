// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_DRAG_SESSION_HPP_
#define EDITOR_WIDGETS_DRAG_SESSION_HPP_

#include <optional>
#include <utility>

namespace editor {

/**
 * @brief "Snapshot before, apply live, commit one undo entry" bookkeeping for viewport gizmo drags.
 *
 * Call tick() every frame with the drag state (e.g. ImGuizmo::IsUsing()); the snapshot callable only runs when a drag starts, so it may be expensive.
 * The instance must live across frames.
 */
template<typename Value>
class drag_session {

public:

  /**
   * @brief Advances the drag state.
   *
   * @param is_using Whether a drag is in progress this frame.
   * @param before_fn Produces the pre-drag snapshot; called only when a drag starts.
   *
   * @return The snapshot on the frame the drag ends, otherwise nullopt. Only brackets undo; apply live values yourself.
   */
  template<typename BeforeFn>
  auto tick(bool is_using, BeforeFn&& before_fn) -> std::optional<Value> {
    if (is_using && !_active) {
      _active = true;
      _before = std::forward<BeforeFn>(before_fn)();
    }

    if (!is_using && _active) {
      _active = false;
      return std::exchange(_before, Value{});
    }

    return std::nullopt;
  }

private:

  bool _active{false};
  Value _before{};

}; // class drag_session

} // namespace editor

#endif // EDITOR_WIDGETS_DRAG_SESSION_HPP_
