// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_GIZMO_HPP_
#define EDITOR_VIEWPORT_GIZMO_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws an ImGuizmo transform gizmo over the selected node, if any, and writes drags back
 * into its local_transform. Must be called while the Viewport window is the current ImGui window
 * (between its Begin/End), since it draws into that window's draw list. W/E/R switch the
 * operation (translate/rotate/scale) while the viewport is hovered.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return true if the cursor is over the gizmo or it's currently being dragged — callers should
 * skip viewport click-to-pick this frame when true, so grabbing a handle doesn't also reselect.
 */
auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

/**
 * @brief Draws a small floating toolbar of translate/rotate/scale buttons over the top-left
 * corner of the viewport (Blender/Unity-style), as a click alternative to the 1/2/3 shortcuts.
 * Only drawn when a node is selected. Must be called while the Viewport window is the current
 * ImGui window, after draw_viewport_gizmo.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 *
 * @return true if the cursor is over the toolbar — callers should skip viewport click-to-pick
 * this frame when true, the same way they already do for the gizmo itself.
 */
auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool;

/**
 * @brief Draws the Blender/Unity/Godot-style camera-orientation cube in the viewport's top-right
 * corner (X/Y/Z colored handles, click a face/axis to snap the view). Writes an updated view back
 * onto the editor camera (editor_module::editor_camera()) when clicked/dragged. Edit-mode only —
 * during Play this would fight whatever's actually driving the scene's own play camera, so it's
 * hidden then. Unlike draw_viewport_gizmo, this isn't tied to node selection. Must be called while
 * the Viewport window is the current ImGui window.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return true if the cursor is over the widget — callers should skip viewport click-to-pick this
 * frame when true, the same way they already do for the other viewport overlays.
 */
auto draw_view_gizmo(const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

/**
 * @brief Draws a clickable icon at the projected screen position of every light and camera node
 * (mesh-less objects with nothing else to represent them in the 3D view) — a light bulb, spotlight,
 * sun, or camera glyph depending on component. Clicking one selects that node, same as clicking a
 * mesh. Always drawn on top (not depth-tested against the scene), like the other viewport overlays.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 * @param gizmo_capturing_input Pass draw_viewport_gizmo's return value for this frame. A selected
 * light/camera's icon projects to the exact same screen point as the gizmo's own center "move on
 * screen plane" handle, and the icon's InvisibleButton would otherwise win ImGui's hover
 * resolution there every frame, permanently blocking ImGuizmo::CanActivate() for that handle. When
 * true, icon hit-testing (hover/click) is skipped for this frame — the glyph itself still draws —
 * so the gizmo gets input priority whenever the cursor is already within its own hotspot.
 *
 * @return true if the cursor is over any icon — callers should skip viewport click-to-pick this
 * frame when true, the same way they already do for the other viewport overlays.
 */
auto draw_node_icons(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size, bool gizmo_capturing_input) -> bool;

/**
 * @brief Draws the selected node's camera view frustum as wireframe lines (near quad, far quad,
 * and the four edges connecting them — the standard Unity/Unreal/Godot camera gizmo) when it has a
 * camera component. Pure visualization submitted into scene_renderer_module's debug_draw
 * accumulator, not ImGui — unlike the other viewport overlays it draws no widgets, captures no
 * input, and has nothing to report back to the caller.
 *
 * @param viewport_size Screen-space size of the viewport image — its aspect ratio shapes the frustum.
 */
auto draw_camera_frustum_gizmo(editor_state& state, const ImVec2& viewport_size) -> void;

} // namespace editor

#endif // EDITOR_VIEWPORT_GIZMO_HPP_
