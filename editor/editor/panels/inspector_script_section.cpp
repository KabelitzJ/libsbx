// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_script_section.hpp>

#include <array>
#include <cstring>
#include <optional>
#include <string>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scripting/scripting_module.hpp>
#include <libsbx/scripting/managed/type.hpp>
#include <libsbx/scripting/managed/field_info.hpp>
#include <libsbx/scripting/managed/attribute.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <editor/commands/component_commands.hpp>

#include <editor/panels/inspector_asset_pickers.hpp>

#include <editor/widgets/vector_fields.hpp>
#include <editor/widgets/layer_fields.hpp>

namespace editor {

auto draw_script_field_inspector(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scenes::script_entry& entry) -> void {
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  auto& type = scripting_module.game_assembly().get_type(entry.class_name);

  if (!type) {
    ImGui::TextDisabled("(script class not found in the compiled assembly — recompile?)");
    return;
  }

  // A live instance exists exactly while playing or paused: instantiate() creates it on Play,
  // run_on_destroy() tears it down on Stop. Must key off instance existence, not
  // scenes_module.is_simulating() (false while paused), or paused edits would target the wrong side.
  auto live_instance = sbx::memory::make_observer<sbx::scripting::managed::object>(nullptr);

  if (node.has_component<sbx::scripting::scripts>()) {
    for (auto& instance : node.get_component<sbx::scripting::scripts>().instances) {
      if (instance.get_type().get_full_name() == entry.class_name) {
        live_instance = &instance;
        break;
      }
    }
  }

  // Backfills a real default for any field that doesn't have a field_overrides entry yet (a script
  // attached before this existed, or recompiled with a new field since — attach_script's own call
  // to this only covers a fresh attach) — see seed_missing_field_defaults' doc comment. A no-op
  // once nothing's missing, and only reachable at all while no live instance exists (a live one
  // already shows its own true values directly via get_field_value below).
  if (!live_instance) {
    scripting_module.seed_missing_field_defaults(node, entry);
  }

  // Shared across every field below — snapshots the whole script_component so undo/redo restores
  // it via modify_component_command<script_component>. Untouched while live_instance is set, since
  // those edits target the live object directly and are never tracked (see the comment above).
  static auto pending = std::optional<sbx::scenes::script_component>{};

  const auto capture_before = [&] {
    if (!live_instance && !pending) {
      pending = node.get_component<sbx::scenes::script_component>();
    }
  };

  const auto commit_after = [&] {
    if (!live_instance && pending) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::script_component>>(node.id(), *pending, node.get_component<sbx::scenes::script_component>(), "Edit Script Field"));
      pending.reset();
    }
  };

  for (auto& field : type.get_fields()) {
    if (field.get_accessibility() != sbx::scripting::managed::type_accessibility::public_access) {
      continue;
    }

    const auto field_name = std::string{field.get_name()};

    auto hidden = false;
    auto display_name = field_name;
    auto is_read_only = false;
    auto has_clamp = false;
    auto clamp_min = 0.0f;
    auto clamp_max = 0.0f;

    for (auto& field_attribute : field.get_attributes()) {
      const auto attribute_type_name = std::string{field_attribute.get_type().get_full_name()};

      if (attribute_type_name == "Sbx.Core.Attributes.HideFromEditorAttribute") {
        hidden = true;
      } else if (attribute_type_name == "Sbx.Core.Attributes.ShowInEditorAttribute") {
        // ShowInEditorAttribute exposes DisplayName/IsReadOnly as auto-properties, not plain
        // fields — read via get_property_value, not get_field_value. An empty DisplayName means
        // the attribute was used without an override (e.g. plain [ShowInEditor]) -- keep the
        // field-name default from above rather than clobbering it with "".
        if (const auto name = field_attribute.get_property_value<std::string>("DisplayName"); !name.empty()) {
          display_name = name;
        }
        is_read_only = field_attribute.get_property_value<bool>("IsReadOnly");
      } else if (attribute_type_name == "Sbx.Core.Attributes.ClampValueAttribute") {
        has_clamp = true;
        clamp_min = static_cast<std::float_t>(field_attribute.get_property_value<std::double_t>("Min"));
        clamp_max = static_cast<std::float_t>(field_attribute.get_property_value<std::double_t>("Max"));
      }
    }

    if (hidden) {
      continue;
    }

    sbx::scenes::script_field_override* override_slot = nullptr;

    for (auto& existing : entry.field_overrides) {
      if (existing.name == field_name) {
        override_slot = &existing;
        break;
      }
    }

    const auto ensure_override_slot = [&]() -> sbx::scenes::script_field_override& {
      if (!override_slot) {
        entry.field_overrides.push_back(sbx::scenes::script_field_override{.name = field_name});
        override_slot = &entry.field_overrides.back();
      }

      return *override_slot;
    };

    ImGui::PushID(field_name.c_str());
    ImGui::BeginDisabled(is_read_only);

    const auto field_type_name = std::string{field.get_type().get_full_name()};

    if (field_type_name == "System.Single") {
      auto value = live_instance ? live_instance->get_field_value<std::float_t>(field_name)
                                  : override_slot ? override_slot->float_value : 0.0f;

      // Always a drag box (click-and-type or click-and-drag), never a slider -- a slider's fixed
      // track width makes fine adjustment across a wide clamp range awkward, whereas a drag box
      // stays precise regardless of range. ClampValue still constrains the result; it just bounds
      // the drag instead of sizing a track.
      const auto changed = has_clamp
        ? ImGui::DragFloat(display_name.c_str(), &value, 0.05f, clamp_min, clamp_max)
        : ImGui::DragFloat(display_name.c_str(), &value, 0.05f);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::float32;
          slot.float_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.Int32") {
      auto value = live_instance ? live_instance->get_field_value<std::int32_t>(field_name)
                                  : override_slot ? override_slot->int_value : 0;

      // Same reasoning as the float branch above -- always a drag box, never a slider.
      const auto changed = has_clamp
        ? ImGui::DragInt(display_name.c_str(), &value, 1.0f, static_cast<std::int32_t>(clamp_min), static_cast<std::int32_t>(clamp_max))
        : ImGui::DragInt(display_name.c_str(), &value);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::int32;
          slot.int_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.Boolean") {
      auto value = live_instance ? live_instance->get_field_value<bool>(field_name)
                                  : override_slot ? override_slot->bool_value : false;

      const auto changed = ImGui::Checkbox(display_name.c_str(), &value);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::boolean;
          slot.bool_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "System.String") {
      const auto current = live_instance ? live_instance->get_field_value<std::string>(field_name)
                                          : override_slot ? override_slot->string_value : std::string{};

      auto buffer = std::array<char, 256u>{};
      std::strncpy(buffer.data(), current.c_str(), buffer.size() - 1u);
      buffer[buffer.size() - 1u] = '\0';

      const auto changed = ImGui::InputText(display_name.c_str(), buffer.data(), buffer.size());

      capture_before();

      if (changed) {
        auto value = std::string{buffer.data()};

        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::string;
          slot.string_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "Sbx.Core.Math.Vector3") {
      const auto current = live_instance ? live_instance->get_field_value<sbx::math::vector3>(field_name)
                                          : override_slot ? override_slot->vector3_value : sbx::math::vector3{0.0f, 0.0f, 0.0f};

      auto values = std::array<std::float_t, 3u>{current.x(), current.y(), current.z()};

      const auto changed = draw_vector3_control(display_name.c_str(), values, 0.0f, 0.05f).changed;

      capture_before();

      if (changed) {
        const auto value = sbx::math::vector3{values[0], values[1], values[2]};

        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::vector3;
          slot.vector3_value = value;
        }
      }

      commit_after();
    } else if (field_type_name == "Sbx.Core.Physics.LayerMask") {
      // A blittable struct (one uint) -- same direct get/set_field_value path as Vector3 above,
      // no reference-type special-casing needed (see components.hpp's script_field_type::layer_mask).
      auto mask = sbx::scenes::layer_mask{live_instance ? live_instance->get_field_value<std::uint32_t>(field_name)
                                                         : override_slot ? override_slot->layer_mask_value : 0xFFFFFFFFu};

      const auto changed = draw_layer_mask_field(state, display_name.c_str(), mask);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, mask.bits);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::layer_mask;
          slot.layer_mask_value = mask.bits;
        }
      }

      commit_after();
    } else if (field_type_name == "Sbx.Core.Node") {
      // A Sbx.Core.Node-typed field is a managed reference, not a value the generic get/set_field_value
      // marshaling can copy -- both directions go through a raw uuid instead, via Node's own
      // INativeHandle implementation (see scripting_module.cpp's _apply_field_overrides and
      // Sbx.Managed's Object.SetFieldValue/Marshalling.MarshalReturnValue).
      const auto current_uuid = live_instance ? live_instance->get_field_value<std::uint64_t>(field_name)
                                                : override_slot ? override_slot->node_value.value() : std::uint64_t{0u};

      auto current_node = (current_uuid != 0u) ? target.find(sbx::math::uuid::from_value(current_uuid)) : sbx::scenes::node{};
      const auto slot_label = current_node.is_valid() ? std::string{current_node.name().c_str()} : std::string{"(none)"};

      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(display_name.c_str());
      ImGui::SameLine(140.0f);

      const auto slot_width = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x;
      ImGui::Button(slot_label.c_str(), ImVec2{slot_width, 0.0f});

      auto new_uuid = std::optional<std::uint64_t>{};

      // Reuses the Hierarchy panel's own node drag payload -- drag a row out of the Hierarchy and
      // drop it here to assign it, same source as reordering nodes there.
      if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload(node_drag_drop_payload_type)) {
          new_uuid = *static_cast<const std::uint64_t*>(payload->Data);
        }

        ImGui::EndDragDropTarget();
      }

      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_CLOSE)) {
        new_uuid = 0u;
      }

      capture_before();

      if (new_uuid) {
        if (live_instance) {
          live_instance->set_field_value(field_name, *new_uuid);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::node;
          slot.node_value = sbx::math::uuid::from_value(*new_uuid);
        }
      }

      commit_after();
    } else if (field_type_name == "Sbx.Core.Material") {
      // Same INativeHandle-via-raw-uuid convention as the Node branch above, backed by the richer
      // asset_picker widget (thumbnail, searchable popup, drag-and-drop from the Asset Browser)
      // rather than a plain drag target, since a .material asset already has that widget for
      // mesh_renderer submesh slots (see inspector_component_sections.cpp) -- allow_none=true here
      // since a script field, unlike a submesh slot, is meaningfully optional.
      auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

      const auto current_uuid = live_instance ? sbx::math::uuid::from_value(live_instance->get_field_value<std::uint64_t>(field_name))
                                                : override_slot ? override_slot->material_value : sbx::math::uuid::nil();

      auto slot = (current_uuid != sbx::math::uuid::nil()) ? assets_module.load_material(current_uuid) : sbx::assets::material_handle{};

      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(display_name.c_str());
      ImGui::SameLine(140.0f);

      const auto changed = draw_material_picker(state, "##material_field_picker", slot, assets_module, {}, true);

      capture_before();

      if (changed) {
        const auto new_uuid = slot.is_valid() ? slot->id() : sbx::math::uuid::nil();

        if (live_instance) {
          live_instance->set_field_value(field_name, new_uuid.value());
        } else {
          auto& slot_override = ensure_override_slot();
          slot_override.type = sbx::scenes::script_field_type::material;
          slot_override.material_value = new_uuid;
        }
      }

      commit_after();
    } else if (field_type_name == "Sbx.Core.Math.Color") {
      // A blittable struct (four sequential floats) -- same direct get/set_field_value path as
      // Vector3 above, no reference-type special-casing needed. draw_color_field is the same
      // ColorEdit4-backed widget the gradient/curve editors already use.
      auto value = live_instance ? live_instance->get_field_value<sbx::math::color>(field_name)
                                  : override_slot ? override_slot->color_value : sbx::math::color{};

      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(display_name.c_str());
      ImGui::SameLine(140.0f);

      const auto changed = draw_color_field("##color_field", value);

      capture_before();

      if (changed) {
        if (live_instance) {
          live_instance->set_field_value(field_name, value);
        } else {
          auto& slot = ensure_override_slot();
          slot.type = sbx::scenes::script_field_type::color;
          slot.color_value = value;
        }
      }

      commit_after();
    } else {
      ImGui::TextDisabled("%s: (unsupported type %s)", display_name.c_str(), field_type_name.c_str());
    }

    ImGui::EndDisabled();
    ImGui::PopID();
  }
}

auto draw_script_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scenes::script_entry& entry, std::optional<std::string>& pending_removal) -> void {
  auto is_open = true;

  const auto title = fmt::format(ICON_MDI_FILE_CODE_OUTLINE " {}.cs", entry.class_name);

  const auto is_expanded = ImGui::CollapsingHeader(title.c_str(), &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    pending_removal = entry.class_name; // defer to after the caller's loop — script_component.scripts must not shrink mid-iteration
    return;
  }

  if (is_expanded) {
    draw_script_field_inspector(state, target, node, entry);
  }
}

} // namespace editor
