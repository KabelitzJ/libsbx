// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_ACTIONS_HPP_
#define EDITOR_EDITOR_ACTIONS_HPP_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>

#include <libsbx/memory/observer_ptr.hpp>

namespace editor {

/** @brief Where an action's shortcut applies: anywhere, or only while a particular part of the editor has focus. */
enum class action_context : std::uint8_t {
  global,
  scene, // the Hierarchy or the Viewport has focus
  viewport, // the Viewport has focus or the mouse is over it
  asset_browser // the Asset Browser has focus
}; // enum class action_context

/**
 * @brief One thing the editor can do: what menus, toolbar buttons, the command palette and shortcuts all invoke, so a label or shortcut is written once.
 *
 * An action without run is polled: triggering it only marks it, and the panel that owns the behaviour consumes the mark (action_registry::consume).
 */
struct editor_action {
  std::string id; // stable, saved with shortcut overrides, e.g. "scene.save"
  std::string label;
  const char* icon{""};
  std::string category;
  std::vector<ImGuiKeyChord> default_chords{};
  action_context context{action_context::global};
  bool works_while_playing{false}; // still fires while the game has the Viewport
  std::function<bool()> is_enabled{};
  std::function<bool()> is_checked{}; // set for toggles: menus show a check mark
  std::function<void()> run{};
  std::function<std::string()> menu_label{}; // replaces label in menus when set, e.g. "Undo Rename"
}; // struct editor_action

/**
 * @brief The readable name of a chord, e.g. "Ctrl+Shift+S".
 *
 * @param chord The chord.
 *
 * @return The name.
 */
[[nodiscard]] auto chord_name(ImGuiKeyChord chord) -> std::string;

/**
 * @brief Parses a name written by chord_name.
 *
 * @param name The name.
 *
 * @return The chord, or nullopt for a name that isn't a modifier set plus one key.
 */
[[nodiscard]] auto parse_chord(std::string_view name) -> std::optional<ImGuiKeyChord>;

/** @brief Every editor action, its current shortcuts (the defaults or the user's overrides) and the per-frame shortcut dispatch. */
class action_registry {

public:

  auto add(editor_action action) -> void;

  /**
   * @brief An action by id.
   *
   * @param id The action's id.
   *
   * @return The action, or null if there's none with that id.
   */
  [[nodiscard]] auto find(std::string_view id) const -> sbx::memory::observer_ptr<const editor_action>;

  [[nodiscard]] auto actions() const noexcept -> const std::vector<editor_action>& {
    return _actions;
  }

  /**
   * @brief An action's shortcuts: the user's override if there is one, otherwise its defaults.
   *
   * @param action The action.
   *
   * @return The chords; empty if it has none.
   */
  [[nodiscard]] auto chords_of(const editor_action& action) const -> std::span<const ImGuiKeyChord>;

  /**
   * @brief The first shortcut's name, for menus and tooltips.
   *
   * @param id The action's id.
   *
   * @return The name, or empty if the action has no shortcut.
   */
  [[nodiscard]] auto shortcut_text(std::string_view id) const -> std::string;

  /**
   * @brief Replaces an action's shortcuts; an empty list means "no shortcut", not "defaults".
   *
   * @param id The action's id.
   * @param chords The new shortcuts.
   */
  auto set_override(std::string_view id, std::vector<ImGuiKeyChord> chords) -> void;

  /** @brief Restores an action's default shortcuts. */
  auto clear_override(std::string_view id) -> void;

  [[nodiscard]] auto overrides() const noexcept -> const std::unordered_map<std::string, std::vector<ImGuiKeyChord>>& {
    return _overrides;
  }

  auto set_overrides(std::unordered_map<std::string, std::vector<ImGuiKeyChord>> overrides) -> void {
    _overrides = std::move(overrides);
  }

  /**
   * @brief Whether an action can run right now.
   *
   * @param action The action.
   *
   * @return True without an is_enabled callback.
   */
  [[nodiscard]] auto is_enabled(const editor_action& action) const -> bool;

  /**
   * @brief Runs an action (or marks a polled one), if it's enabled, and remembers it for the command palette.
   *
   * @param id The action's id.
   */
  auto run(std::string_view id) -> void;

  /**
   * @brief Takes a polled action's mark, set by run or a shortcut this frame or the last.
   *
   * @param id The action's id.
   *
   * @return Whether it was triggered.
   */
  [[nodiscard]] auto consume(std::string_view id) -> bool;

  /**
   * @brief Runs the action whose shortcut was pressed this frame, among those whose context is active.
   *
   * Nothing runs while a text field takes input or a shortcut is being recorded; while the game has the Viewport, only works_while_playing actions do.
   *
   * @param active_contexts The contexts with focus this frame (global is always implied).
   * @param game_has_viewport Whether a running game has the Viewport's focus or the mouse.
   */
  auto dispatch(std::span<const action_context> active_contexts, bool game_has_viewport) -> void;

  /**
   * @brief A menu item for an action: icon, label, shortcut, enabled and checked state; runs it when clicked.
   *
   * @param id The action's id.
   */
  auto menu_item(std::string_view id) -> void;

  /**
   * @brief A tooltip for the hovered item naming an action and its shortcut, e.g. "Move Tool (W)".
   *
   * @param id The action's id.
   */
  auto item_tooltip(std::string_view id) const -> void;

  /**
   * @brief The actions run most recently, newest first.
   *
   * @return Their ids.
   */
  [[nodiscard]] auto recent() const noexcept -> const std::vector<std::string>& {
    return _recent;
  }

  /** @brief Suspends dispatch while the Shortcuts page records a chord, so pressing it doesn't also run it. */
  auto set_capturing(bool capturing) noexcept -> void {
    _is_capturing = capturing;
  }

private:

  struct trigger {
    std::string id;
    int frame{0};
  }; // struct trigger

  std::vector<editor_action> _actions{};
  std::unordered_map<std::string, std::vector<ImGuiKeyChord>> _overrides{};
  std::vector<trigger> _triggers{};
  std::vector<std::string> _recent{};
  bool _is_capturing{false};

}; // class action_registry

} // namespace editor

#endif // EDITOR_EDITOR_ACTIONS_HPP_
