// SPDX-License-Identifier: MIT
#include <editor/widgets/controls.hpp>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstring>

namespace editor {

auto controls::vector3(const std::string& label, sbx::math::vector3& values, std::float_t reset_value, std::float_t speed) -> bool {
  auto has_changed = false;

  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, 80.0f);
  ImGui::Text("%s", label.c_str());
  ImGui::NextColumn();

  auto region = ImGui::GetContentRegionAvail().x;
  auto spacing = 4.0f;
  auto line_height = ImGui::GetFrameHeight();
  auto button_width = line_height;
  auto item_width = std::max(40.0f, (region - button_width * 3.0f - spacing * 2.0f) / 3.0f);
  auto button_size = ImVec2{button_width, line_height};

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 0.0f});

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.7f, 0.15f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.85f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.7f, 0.15f, 0.15f, 1.0f});

  if (ImGui::Button("X", button_size)) {
    values.x() = reset_value;
    has_changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto x = values.x();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##x", &x, speed, 0.0f, 0.0f, "%.3f")) {
    values.x() = x;
    has_changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::SameLine(0.0f, spacing);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.6f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.75f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.6f, 0.15f, 1.0f});

  if (ImGui::Button("Y", button_size)) {
    values.y() = reset_value;
    has_changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto y = values.y();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##y", &y, speed, 0.0f, 0.0f, "%.3f")) {
    values.y() = y;
    has_changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::SameLine(0.0f, spacing);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.15f, 0.7f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.2f, 0.85f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.15f, 0.7f, 1.0f});

  if (ImGui::Button("Z", button_size)) {
    values.z() = reset_value;
    has_changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto z = values.z();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##z", &z, speed, 0.0f, 0.0f, "%.3f")) {
    values.z() = z;
    has_changed = true;
  }

  ImGui::PopItemWidth();

  ImGui::PopStyleVar();
  ImGui::Columns(1);

  ImGui::PopID();

  return has_changed;
}

auto controls::color3(const char* label, sbx::math::color& color) -> bool {
  auto values = std::array<std::float_t, 3>{color.r(), color.g(), color.b()};

  if (ImGui::ColorEdit3(label, values.data())) {
    color.r() = values[0];
    color.g() = values[1];
    color.b() = values[2];

    return true;
  }

  return false;
}

auto controls::color4(const char* label, sbx::math::color& color) -> bool {
  auto values = std::array<std::float_t, 4>{color.r(), color.g(), color.b(), color.a()};

  if (ImGui::ColorEdit4(label, values.data())) {
    color.r() = values[0];
    color.g() = values[1];
    color.b() = values[2];
    color.a() = values[3];

    return true;
  }

  return false;
}

auto controls::reset_drag_float(const char* label, std::float_t& value, std::float_t reset_value, std::float_t speed, std::float_t min_value, std::float_t max_value, const char* format) -> bool {
  auto has_changed = false;

  ImGui::PushID(label);

  const auto line_height = ImGui::GetFrameHeight();
  const auto button_size = ImVec2{line_height, line_height};

  if (ImGui::Button("R", button_size)) {
    value = reset_value;
    has_changed = true;
  }

  ImGui::SameLine();

  if (ImGui::DragFloat(label, &value, speed, min_value, max_value, format)) {
    has_changed = true;
  }

  ImGui::PopID();

  return has_changed;
}

auto controls::texture_slot(const char* label, sbx::models::texture_slot& slot, texture_cache& cache, const char* drag_drop_payload) -> void {
  ImGui::PushID(label);

  const auto slot_size = 64.0f;
  const auto descriptor_set = cache.descriptor_for(slot.image);

  ImGui::BeginGroup();

  if (descriptor_set != VK_NULL_HANDLE) {
    ImGui::ImageButton("##thumb", reinterpret_cast<ImTextureID>(descriptor_set), ImVec2{slot_size, slot_size});
  } else {
    ImGui::Button(ICON_MDI_IMAGE_PLUS "##slot", ImVec2{slot_size, slot_size});
  }

  if (ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload(drag_drop_payload)) {
      auto handle = sbx::graphics::image2d_handle{};
      std::memcpy(&handle, payload->Data, sizeof(sbx::graphics::image2d_handle));
      slot.image = handle;
    }

    ImGui::EndDragDropTarget();
  }

  if (slot.image.is_valid() && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    slot.image = sbx::graphics::image2d_handle{};
  }

  ImGui::EndGroup();

  ImGui::SameLine();

  ImGui::BeginGroup();

  ImGui::Text("%s", label);

  if (slot.image.is_valid()) {
    ImGui::TextDisabled("Handle: %u", static_cast<std::uint32_t>(slot.image.handle()));
  } else {
    ImGui::TextDisabled("(empty)");
  }

  ImGui::SetNextItemWidth(120.0f);
  enum_combo("Mag Filter", slot.mag_filter);

  ImGui::SetNextItemWidth(120.0f);
  enum_combo("Min Filter", slot.min_filter);

  ImGui::SetNextItemWidth(120.0f);
  enum_combo("Wrap U", slot.address_mode_u);

  ImGui::SetNextItemWidth(120.0f);
  enum_combo("Wrap V", slot.address_mode_v);

  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat("Anisotropy", &slot.anisotropy, 0.5f, 1.0f, 16.0f, "%.1f");

  ImGui::EndGroup();

  ImGui::Separator();

  ImGui::PopID();
}

} // namespace editor
