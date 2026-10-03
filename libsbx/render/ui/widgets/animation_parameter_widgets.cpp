// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/animation_parameter_widgets.hpp>

#include <array>
#include <cstdint>
#include <type_traits>
#include <variant>

#include <imgui.h>

namespace sbx::render {

auto draw_animation_parameter_value(const char* label, sbx::assets::animation_parameter_value& value) -> bool {
  auto changed = false;

  std::visit([&](auto& current) {
    using value_type = std::decay_t<decltype(current)>;

    if constexpr (std::is_same_v<value_type, std::float_t>) {
      changed = ImGui::DragFloat(label, &current, 0.05f);
    } else if constexpr (std::is_same_v<value_type, bool>) {
      // A True/False dropdown reads better next to the other authored-value widgets; the Inspector's live testing UI uses a checkbox.
      static constexpr auto bool_names = std::array<const char*, 2u>{"False", "True"};
      auto index = current ? 1 : 0;

      if (ImGui::Combo(label, &index, bool_names.data(), static_cast<std::int32_t>(bool_names.size()))) {
        current = (index == 1);
        changed = true;
      }
    } else if constexpr (std::is_same_v<value_type, std::int32_t>) {
      changed = ImGui::DragInt(label, &current);
    } else {
      static_assert(std::is_same_v<value_type, sbx::assets::animation_trigger>);
      ImGui::BeginDisabled();
      ImGui::TextUnformatted("(Trigger)");
      ImGui::EndDisabled();
    }
  }, value);

  return changed;
}

auto default_for_same_alternative(const sbx::assets::animation_parameter_value& like) -> sbx::assets::animation_parameter_value {
  return std::visit([](const auto& current) -> sbx::assets::animation_parameter_value {
    using value_type = std::decay_t<decltype(current)>;
    return value_type{};
  }, like);
}

} // namespace sbx::render
