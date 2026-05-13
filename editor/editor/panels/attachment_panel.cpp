// SPDX-License-Identifier: MIT
#include <editor/panels/attachment_panel.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

static const auto attachment_names = std::vector<std::string_view>{
  "albedo",
  "position",
  "material",
  "emissive",
  "linear_depth",
  "accumulator",
  "revealage",
  "resolve",
  "brightness",
  "tonemap",
  "fxaa"
};

attachment_panel::attachment_panel()
: _current_attachment_name{attachment_names.size() - 1u},
  _attachment_name{attachment_names[_current_attachment_name]} { } 

attachment_panel::~attachment_panel() {
  if (_texture_id != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplVulkan_RemoveTexture(_texture_id);
  }
}

auto attachment_panel::draw() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  ImGui::Begin(ICON_MDI_LAYERS_TRIPLE_OUTLINE " Attachment###attachment_panel");

  ImGui::Text("Render Attachment");

  if (ImGui::BeginCombo("##RenderAttachment", attachment_names[_current_attachment_name].data())) {
    for (auto i = 0u; i < attachment_names.size(); ++i) {
      const auto is_selected = (_current_attachment_name == i);

      if (ImGui::Selectable(attachment_names[i].data(), is_selected)) {
        _current_attachment_name = i;
        _attachment_name = attachment_names[i];
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  auto& attachment_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment(_attachment_name));

  _update_texture(attachment_image);

  const auto available = ImGui::GetContentRegionAvail();

  if (_texture_id != VK_NULL_HANDLE) {
    ImGui::Image(reinterpret_cast<ImTextureID>(_texture_id), ImVec2{available.x, available.x});
  }

  ImGui::End();
}

auto attachment_panel::_update_texture(const sbx::graphics::image2d& image) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto current_view = image.view();

  if (current_view == _cached_view) {
    return;
  }

  if (_texture_id != VK_NULL_HANDLE) {
    graphics_module.enqueue_destruction([handle = _texture_id]([[maybe_unused]] auto& allocator){
      ImGui_ImplVulkan_RemoveTexture(handle);
    });
  }

  _texture_id = ImGui_ImplVulkan_AddTexture(image.sampler(), current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  _cached_view = current_view;
}

} // namespace editor