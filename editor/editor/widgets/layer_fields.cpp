// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/widgets/layer_fields.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

namespace editor {

auto draw_layer_combo(editor_state& state, const char* label, std::uint8_t& layer_index) -> bool {
  auto& project = sbx::core::engine::project();

  auto changed = false;

  const auto& current_name = project.layer_name(layer_index);
  const auto current_label = current_name.empty() ? fmt::format("Layer {}", layer_index) : current_name;

  if (ImGui::BeginCombo(label, current_label.c_str())) {
    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      const auto& name = project.layers()[index];

      if (name.empty()) {
        continue; // unnamed slot -- not offered, same as Unity hiding empty layers from the picker
      }

      const auto is_selected = index == layer_index;

      if (ImGui::Selectable(name.c_str(), is_selected)) {
        layer_index = static_cast<std::uint8_t>(index);
        changed = true;
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::Separator();

    if (ImGui::Selectable("Edit Layers...")) {
      state.request_open_edit_layers_popup();
    }

    ImGui::EndCombo();
  }

  return changed;
}

auto draw_layer_mask_field(editor_state& state, const char* label, sbx::scenes::layer_mask& mask) -> bool {
  auto& project = sbx::core::engine::project();

  auto changed = false;

  auto included_names = std::vector<std::string>{};

  for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
    const auto& name = project.layers()[index];

    if (!name.empty() && mask.test(static_cast<std::uint8_t>(index))) {
      included_names.push_back(name);
    }
  }

  const auto preview = [&] {
    if (mask.bits == 0xFFFFFFFFu) {
      return std::string{"Everything"};
    }

    if (mask.bits == 0u) {
      return std::string{"Nothing"};
    }

    if (included_names.empty()) {
      return std::string{"Mixed"}; // only unnamed layers are set -- nothing named to list
    }

    if (included_names.size() <= 3u) {
      auto joined = included_names.front();

      for (auto index = std::size_t{1u}; index < included_names.size(); ++index) {
        joined += ", " + included_names[index];
      }

      return joined;
    }

    return fmt::format("Mixed ({})", included_names.size());
  }();

  ImGui::PushID(label);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(140.0f);
  ImGui::Button(preview.c_str(), ImVec2{-1.0f, 0.0f});

  if (ImGui::IsItemClicked()) {
    ImGui::OpenPopup("##layer_mask_popup");
  }

  if (ImGui::BeginPopup("##layer_mask_popup")) {
    if (ImGui::Selectable("Nothing")) {
      mask = sbx::scenes::layer_mask::none();
      changed = true;
    }

    if (ImGui::Selectable("Everything")) {
      mask = sbx::scenes::layer_mask::everything();
      changed = true;
    }

    ImGui::Separator();

    for (auto index = std::size_t{0u}; index < sbx::core::layer_count; ++index) {
      const auto& name = project.layers()[index];

      if (name.empty()) {
        continue;
      }

      auto is_set = mask.test(static_cast<std::uint8_t>(index));

      if (ImGui::Checkbox(name.c_str(), &is_set)) {
        if (is_set) {
          mask.set(static_cast<std::uint8_t>(index));
        } else {
          mask.clear(static_cast<std::uint8_t>(index));
        }

        changed = true;
      }
    }

    ImGui::Separator();

    if (ImGui::Selectable("Edit Layers...")) {
      state.request_open_edit_layers_popup();
    }

    ImGui::EndPopup();
  }

  ImGui::PopID();

  return changed;
}

} // namespace editor
