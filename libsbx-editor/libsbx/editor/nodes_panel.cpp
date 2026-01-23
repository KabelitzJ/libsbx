// SPDX-License-Identifier: MIT
#include <libsbx/editor/nodes_panel.hpp>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
// #include <ImGuizmo.h>

namespace sbx::editor {

nodes_panel::nodes_panel() {

}

auto nodes_panel::render() -> void {
  ImGui::Begin("Nodes");

  ImNodes::BeginNodeEditor();

  static auto color = ImVec4{1.0f, 0.0f, 0.0f, 1.0f};

  if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup("editor_context_menu");
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

  if (ImGui::BeginPopup("editor_context_menu")) {
    if (ImGui::MenuItem("Color")) {
      
    }
    if (ImGui::MenuItem("Add")) {

    }
    if (ImGui::MenuItem("Subtract")) {

    }
    if (ImGui::MenuItem("Multiply")) {

    }
    if (ImGui::MenuItem("Remap")) {

    }
    ImGui::EndPopup();
  }

  ImGui::PopStyleVar();

  ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(250, 0, 0, 255));
  ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(255, 0, 0, 255));
  ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(255, 0, 0, 255));
  ImNodes::BeginNode(0);

  ImNodes::BeginNodeTitleBar();
  ImGui::TextUnformatted("Color");
  ImNodes::EndNodeTitleBar();

  ImNodes::BeginOutputAttribute(1);
  ImGui::SetNextItemWidth(150);
  ImGui::DragFloat("##red", &color.x, 0.001f, 0.0f, 1.0f);
  ImGui::SameLine();
  ImGui::Text("  Red");
  ImNodes::EndOutputAttribute();  

  ImNodes::BeginOutputAttribute(2);
  ImGui::SetNextItemWidth(150);
  ImGui::DragFloat("##green", &color.y, 0.001f, 0.0f, 1.0f);
  ImGui::SameLine();
  ImGui::Text("Green");
  ImNodes::EndOutputAttribute();

  ImNodes::BeginOutputAttribute(3);
  ImGui::SetNextItemWidth(150);
  ImGui::DragFloat("##blue", &color.z, 0.001f, 0.0f, 1.0f);
  ImGui::SameLine();
  ImGui::Text(" Blue");
  ImNodes::EndOutputAttribute();

  ImNodes::BeginOutputAttribute(4);
  ImGui::SetNextItemWidth(150);
  ImGui::DragFloat("##alpha", &color.w, 0.001f, 0.0f, 1.0f);
  ImGui::SameLine();
  ImGui::Text("Alpha");
  ImNodes::EndOutputAttribute();

  ImGui::Dummy(ImVec2(0.0f, 10.0f));

  ImGui::SetNextItemWidth(200);
  ImGui::ColorPicker4("##color_picker", &color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview);

  ImNodes::EndNode();
  ImNodes::PopColorStyle();
  ImNodes::PopColorStyle();
  ImNodes::PopColorStyle();

  ImNodes::BeginNode(5);

  ImNodes::BeginNodeTitleBar();
  ImGui::TextUnformatted("add");
  ImNodes::EndNodeTitleBar();

  ImNodes::BeginInputAttribute(6);
  ImGui::Text("in");
  ImNodes::EndInputAttribute();

  ImGui::Dummy(ImVec2(20.0f, 0.0f));

  ImNodes::BeginOutputAttribute(7);
  ImGui::Text("out");
  ImNodes::EndOutputAttribute();

  ImNodes::BeginInputAttribute(8);
  ImGui::Text("in");
  ImNodes::EndInputAttribute();

  ImNodes::EndNode();

  for (auto i = 0u; i < _links.size(); ++i) {
    const auto link = _links[i];
    ImNodes::Link(static_cast<std::int32_t>(i), link.start, link.end);
  }

  ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);

  ImNodes::EndNodeEditor();

  auto start = std::int32_t{0};
  auto end = std::int32_t{0};

  if (ImNodes::IsLinkCreated(&start, &end)) {
    _links.emplace_back(start, end);
  }

  ImGui::End();
}

} // namespace sbx::editor