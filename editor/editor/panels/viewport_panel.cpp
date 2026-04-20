// SPDX-License-Identifier: MIT
#include <editor/panels/viewport_panel.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

viewport_panel::~viewport_panel() {
  if (_texture_id != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplVulkan_RemoveTexture(_texture_id);
  }
}

auto viewport_panel::draw(const sbx::graphics::image2d& scene_image) -> void {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin("Viewport");
  ImGui::PopStyleVar();

  _is_focused = ImGui::IsWindowFocused();
  _is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  _panel_size = sbx::math::vector2u{width, height};

  _update_texture(scene_image);

  if (_texture_id != VK_NULL_HANDLE) {
    ImGui::Image(reinterpret_cast<ImTextureID>(_texture_id), available);
  }

  ImGui::End();
}

auto viewport_panel::_update_texture(const sbx::graphics::image2d& image) -> void {
  auto current_view = image.view();

  if (current_view == _cached_view) {
    return;
  }

  if (_texture_id != VK_NULL_HANDLE) {
    ImGui_ImplVulkan_RemoveTexture(_texture_id);
  }

  _texture_id = ImGui_ImplVulkan_AddTexture(image.sampler(), current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  _cached_view = current_view;
}

} // namespace editor
