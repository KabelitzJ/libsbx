// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_DRAG_SESSION_HPP_
#define EDITOR_WIDGETS_DRAG_SESSION_HPP_

#include <optional>
#include <utility>

namespace editor::widgets {

/**
 * @brief Cross-frame "capture before, apply live, commit as one undo entry" bookkeeping shared by
 * every viewport gizmo drag (single-node transform, group-pivot transform, ...): call tick() once
 * per frame with whatever library call reports "is a drag in progress" (e.g. ImGuizmo::IsUsing())
 * and a callable producing the pre-drag snapshot to capture -- only invoked on the false->true
 * edge, so it's safe for the snapshot to be expensive to build (a whole selection's worth of
 * transforms, say).
 *
 * A caller instance must be function-local static (or otherwise live across frames) — one call
 * site of ImGuizmo::IsUsing() at a time is enough since only one gizmo can be mid-drag.
 */
template<typename Value>
class drag_session {

public:

  /**
   * @return The captured pre-drag value the instant the drag ends (the true->false edge on this
   * call); nullopt every other frame, including every frame while the drag is still in progress —
   * apply the live value directly from the caller's own per-frame result, this only brackets undo.
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

} // namespace editor::widgets

#endif // EDITOR_WIDGETS_DRAG_SESSION_HPP_
