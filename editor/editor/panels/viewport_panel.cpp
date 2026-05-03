// SPDX-License-Identifier: MIT
#include <editor/panels/viewport_panel.hpp>

#include <editor/bindings/imgui.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/buffer.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/images/image.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/scene_environment.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/relationship.hpp>

#include <libsbx/math/matrix_cast.hpp>

namespace editor {

viewport_panel::~viewport_panel() {
  if (_texture_id != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplVulkan_RemoveTexture(_texture_id);
  }
}

auto viewport_panel::draw(const sbx::graphics::image2d& scene_image, sbx::scenes::node selected_node) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  if (!scenes_module.has_active_scene()) {
    return;
  }

  auto& scene = scenes_module.active_scene();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();

  _is_focused = ImGui::IsWindowFocused();
  _is_hovered = ImGui::IsWindowHovered();

  _draw_toolbar();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  _panel_size = sbx::math::vector2u{width, height};

  auto cursor = ImGui::GetCursorScreenPos();
  _content_min = sbx::math::vector2{cursor.x, cursor.y};

  _update_texture(scene_image);

  if (_texture_id != VK_NULL_HANDLE) {
    ImGui::Image(reinterpret_cast<ImTextureID>(_texture_id), available);
  }

  _handle_picking();

  _draw_gizmo(scene, selected_node);

  ImGui::End();
}

auto viewport_panel::_handle_picking() -> void {
  if (!_is_hovered) {
    return;
  }

  if (is_gizmo_active()) {
    return;
  }

  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    return;
  }

  const auto mouse = ImGui::GetMousePos();

  const auto local_x = mouse.x - _content_min.x();
  const auto local_y = mouse.y - _content_min.y();

  if (local_x < 0.0f || local_y < 0.0f) {
    return;
  }

  if (local_x >= static_cast<std::float_t>(_panel_size.x()) || local_y >= static_cast<std::float_t>(_panel_size.y())) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  const auto& object_id_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment("object_id"));
  const auto& image_size = object_id_image.size();

  const auto pixel_x = static_cast<std::uint32_t>(local_x / static_cast<std::float_t>(_panel_size.x()) * static_cast<std::float_t>(image_size.x()));
  const auto pixel_y = static_cast<std::uint32_t>(local_y / static_cast<std::float_t>(_panel_size.y()) * static_cast<std::float_t>(image_size.y()));

  if (pixel_x >= image_size.x() || pixel_y >= image_size.y()) {
    return;
  }

  const auto object_id = _read_object_id(object_id_image, pixel_x, pixel_y);

  if (!object_id.has_value()) {
    return;
  }

  if (*object_id == 0xFFFFFFFFu) {
    _picked_node = sbx::scenes::node::null;
    return;
  }

  _picked_node = static_cast<sbx::scenes::node>(*object_id);
}

auto viewport_panel::_read_object_id(const sbx::graphics::image2d& image, std::uint32_t pixel_x, std::uint32_t pixel_y) -> std::optional<std::uint32_t> {
  auto staging = sbx::graphics::buffer{sizeof(std::uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  auto command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  const auto subresource_range = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  // Acquire: wait for color writes, transition to TRANSFER_SRC

  auto pre_barrier = VkImageMemoryBarrier2{};
  pre_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  pre_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  pre_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  pre_barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  pre_barrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  pre_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  pre_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
  pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  pre_barrier.image = image;
  pre_barrier.subresourceRange = subresource_range;

  auto pre_dependency = VkDependencyInfo{};
  pre_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  pre_dependency.imageMemoryBarrierCount = 1;
  pre_dependency.pImageMemoryBarriers = &pre_barrier;

  vkCmdPipelineBarrier2(command_buffer, &pre_dependency);

  // Copy

  auto region = VkBufferImageCopy{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = VkOffset3D{static_cast<std::int32_t>(pixel_x), static_cast<std::int32_t>(pixel_y), 0};
  region.imageExtent = VkExtent3D{1, 1, 1};

  vkCmdCopyImageToBuffer(command_buffer, image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.handle(), 1, &region);

  // Release: transition back to SHADER_READ_ONLY

  auto post_barrier = VkImageMemoryBarrier2{};
  post_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  post_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  post_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  post_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  post_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
  post_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  post_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;         
  post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  post_barrier.image = image;
  post_barrier.subresourceRange = subresource_range;

  auto post_dependency = VkDependencyInfo{};
  post_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  post_dependency.imageMemoryBarrierCount = 1;
  post_dependency.pImageMemoryBarriers = &post_barrier;

  vkCmdPipelineBarrier2(command_buffer, &post_dependency);

  command_buffer.submit_idle();

  staging.map();

  auto value = std::uint32_t{0};
  std::memcpy(&value, staging.mapped_memory().get(), sizeof(std::uint32_t));

  staging.unmap();

  return value;
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

auto viewport_panel::_draw_toolbar() -> void {
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
      _gizmo_operation = ImGuizmo::TRANSLATE;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
      _gizmo_operation = ImGuizmo::ROTATE;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
      _gizmo_operation = ImGuizmo::SCALE;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
      if (_gizmo_operation == ImGuizmo::TRANSLATE) {
        _gizmo_operation = ImGuizmo::ROTATE;
      } else if (_gizmo_operation == ImGuizmo::ROTATE) {
        _gizmo_operation = ImGuizmo::SCALE;
      } else {
        _gizmo_operation = ImGuizmo::TRANSLATE;
      }
    }
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8.0f, 4.0f});

  if (ImGui::RadioButton("Translate", _gizmo_operation == ImGuizmo::TRANSLATE)) {
    _gizmo_operation = ImGuizmo::TRANSLATE;
  }

  ImGui::SameLine();

  if (ImGui::RadioButton("Rotate", _gizmo_operation == ImGuizmo::ROTATE)) {
    _gizmo_operation = ImGuizmo::ROTATE;
  }

  ImGui::SameLine();

  if (ImGui::RadioButton("Scale", _gizmo_operation == ImGuizmo::SCALE)) {
    _gizmo_operation = ImGuizmo::SCALE;
  }

  if (_gizmo_operation != ImGuizmo::SCALE) {
    ImGui::SameLine();

    if (ImGui::RadioButton("Local", _gizmo_mode == ImGuizmo::LOCAL)) {
      _gizmo_mode = ImGuizmo::LOCAL;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("World", _gizmo_mode == ImGuizmo::WORLD)) {
      _gizmo_mode = ImGuizmo::WORLD;
    }
  }

  ImGui::PopStyleVar();

  ImGui::Separator();
}

auto viewport_panel::_draw_gizmo(sbx::scenes::scene& scene, sbx::scenes::node selected_node) -> void {
  if (selected_node == sbx::scenes::node::null) {
    return;
  }

  auto& graph = scene.graph();
  auto& environment = scene.environment();

  if (!graph.is_valid(selected_node)) {
    return;
  }

  if (!graph.has_component<sbx::scenes::transform>(selected_node)) {
    return;
  }

  auto camera_node = environment.camera();

  if (camera_node == sbx::scenes::node::null) {
    return;
  }

  auto view_matrix = sbx::math::matrix4x4::inverted(graph.world_transform(camera_node));

  auto& camera = graph.get_component<sbx::scenes::camera>(camera_node);

  auto size = environment.render_target_size();
  auto aspect = size.y() == 0u ? 1.0f : static_cast<std::float_t>(size.x()) / static_cast<std::float_t>(size.y());

  auto projection_matrix = camera.projection(aspect);
  projection_matrix[1][1] *= -1.0f;

  auto world_matrix = graph.world_transform(selected_node);

  auto view = std::array<std::float_t, 16>{};
  auto projection = std::array<std::float_t, 16>{};
  auto world = std::array<std::float_t, 16>{};

  _to_imguizmo(view_matrix, view);
  _to_imguizmo(projection_matrix, projection);
  _to_imguizmo(world_matrix, world);

  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(_content_min.x(), _content_min.y(), static_cast<std::float_t>(_panel_size.x()), static_cast<std::float_t>(_panel_size.y()));

  if (!ImGuizmo::Manipulate(view.data(), projection.data(), _gizmo_operation, _gizmo_mode, world.data())) {
    return;
  }

  auto new_world = _from_imguizmo(world);
  auto new_local = new_world;

  const auto& relationship = graph.get_component<sbx::scenes::relationship>(selected_node);
  auto parent = relationship.parent();

  if (parent != sbx::scenes::node::null) {
    auto parent_world = graph.world_transform(parent);
    new_local = sbx::math::matrix4x4::inverted(parent_world) * new_world;
  }

  auto local_matrix = std::array<std::float_t, 16>{};
  _to_imguizmo(new_local, local_matrix);

  auto translation = std::array<std::float_t, 3>{};
  auto rotation = std::array<std::float_t, 3>{};
  auto scale = std::array<std::float_t, 3>{};

  ImGuizmo::DecomposeMatrixToComponents(local_matrix.data(), translation.data(), rotation.data(), scale.data());

  auto& transform = graph.get_component<sbx::scenes::transform>(selected_node);

  transform.set_position({translation[0], translation[1], translation[2]});
  transform.set_rotation(sbx::math::vector3{rotation[0], rotation[1], rotation[2]});
  transform.set_scale({scale[0], scale[1], scale[2]});
}

auto viewport_panel::_to_imguizmo(const sbx::math::matrix4x4& source, std::span<std::float_t, 16u> destination) -> void {
  for (auto column = 0u; column < 4u; ++column) {
    for (auto row = 0u; row < 4u; ++row) {
      destination[column * 4u + row] = static_cast<std::float_t>(source[column][row]);
    }
  }
}

auto viewport_panel::_from_imguizmo(std::span<const std::float_t, 16u> source) -> sbx::math::matrix4x4 {
  auto result = sbx::math::matrix4x4::identity;

  for (auto column = 0u; column < 4u; ++column) {
    for (auto row = 0u; row < 4u; ++row) {
      result[column][row] = source[column * 4u + row];
    }
  }

  return result;
}

} // namespace editor
