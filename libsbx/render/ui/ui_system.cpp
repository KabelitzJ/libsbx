// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/ui_system.hpp>

#include <algorithm>

#include <libsbx/render/ui/backends/v1.92.9-docking/imgui_impl_glfw.h>
#include <libsbx/render/ui/backends/v1.92.9-docking/imgui_impl_vulkan.h>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/devices/surface.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

ui_system::ui_system() {
  auto& platform_module = core::engine::get_module<platform::platform_module>();
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui_ImplGlfw_InitForVulkan(platform_module.window().handle(), true);

  auto& logical_device = graphics_module.logical_device();
  auto& graphics_queue = logical_device.queue<graphics::queue::type::graphics>();
  auto& surface = graphics_module.surface();

  auto surface_format = surface.format().format;
  auto surface_capabilities = surface.capabilities();

  auto image_count = surface_capabilities.minImageCount + 1u;

  if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  auto init_info = ImGui_ImplVulkan_InitInfo{};
  init_info.Instance = graphics_module.instance();
  init_info.PhysicalDevice = graphics_module.physical_device();
  init_info.Device = logical_device;
  init_info.QueueFamily = graphics_queue.family();
  init_info.Queue = graphics_queue;
  init_info.MinImageCount = graphics::swapchain::max_frames_in_flight;
  init_info.ImageCount = image_count;
  init_info.DescriptorPoolSize = 64u;
  init_info.UseDynamicRendering = true;
  init_info.MinAllocationSize = 1024 * 1024;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfo{};
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1u;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &surface_format;

  ImGui_ImplVulkan_Init(&init_info);
}

ui_system::~ui_system() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  graphics_module.logical_device().wait_idle();

  // The device is fully idle here, so every pending free below is safe to flush unconditionally
  // rather than waiting on its retirement frame.
  for (auto& [descriptor_set, frame_index] : _pending_texture_frees) {
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);
  }

  for (auto& [view, entry] : _textures) {
    ImGui_ImplVulkan_RemoveTexture(entry.descriptor_set);
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

auto ui_system::add_layer(memory::observer_ptr<ui_layer> layer) -> void {
  _layers.push_back(layer);
}

auto ui_system::remove_layer(memory::observer_ptr<ui_layer> layer) -> void {
  std::erase(_layers, layer);
}

auto ui_system::add_font(const std::filesystem::path& path, std::float_t size_pixels) -> ImFont* {
  return fonts()->AddFontFromFileTTF(path.string().c_str(), size_pixels);
}

auto ui_system::build_frame() -> ui_draw_data {
  SBX_PROFILE_SCOPE("ui_system::build_frame");

  _collect_pending_textures();

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  for (auto layer : _layers) {
    layer->build();
  }

  ImGui::Render();

  return ui_draw_data{ImGui::GetDrawData()};
}

auto ui_system::render(render_context& context, const ui_draw_data& data) -> void {
  if (!data.is_valid()) {
    return;
  }

  SBX_PROFILE_SCOPE("ui_system::render");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& swapchain = graphics_module.frame_context().swapchain();

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain.active_image_view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // the composite pass already gave this frame a background.
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.swapchain_extent.x(), context.swapchain_extent.y()}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;

  context.command_buffer->begin_rendering(rendering_info);

  // Reconstruct a real ImDrawData over the deep copy. CmdLists is a plain ImVector whose
  // destructor unconditionally IM_FREE()s its Data pointer; pointing entry straight at ui_draw_data's
  // own storage (no copy) is safe only because Data/Size/Capacity are detached again below, before
  // this local's destructor can run on memory entry never allocated.
  auto draw_data = ImDrawData{};
  draw_data.Valid = true;
  draw_data.CmdListsCount = static_cast<int>(data.draw_lists().size());
  draw_data.CmdLists.Data = const_cast<ImDrawList**>(data.draw_lists().data());
  draw_data.CmdLists.Size = draw_data.CmdListsCount;
  draw_data.CmdLists.Capacity = draw_data.CmdListsCount;
  draw_data.TotalVtxCount = data.total_vertex_count();
  draw_data.TotalIdxCount = data.total_index_count();
  draw_data.DisplayPos = ImVec2{data.display_pos().x(), data.display_pos().y()};
  draw_data.DisplaySize = ImVec2{data.display_size().x(), data.display_size().y()};
  draw_data.FramebufferScale = ImVec2{data.framebuffer_scale().x(), data.framebuffer_scale().y()};
  draw_data.OwnerViewport = ImGui::GetMainViewport();
  draw_data.Textures = data.textures();

  ImGui_ImplVulkan_RenderDrawData(&draw_data, *context.command_buffer);

  draw_data.CmdLists.Data = nullptr;
  draw_data.CmdLists.Size = 0;
  draw_data.CmdLists.Capacity = 0;

  context.command_buffer->end_rendering();
}

auto ui_system::texture_id(VkImageView view, VkSampler sampler) -> ImTextureID {
  auto entry = _textures.find(view);

  if (entry != _textures.end() && entry->second.sampler == sampler) {
    return reinterpret_cast<ImTextureID>(entry->second.descriptor_set);
  }

  if (entry != _textures.end()) {
    _retire_texture(entry->second.descriptor_set);
    _textures.erase(entry);
  }

  const auto descriptor_set = ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  _textures.emplace(view, texture_entry{descriptor_set, sampler});

  return reinterpret_cast<ImTextureID>(descriptor_set);
}

auto ui_system::_retire_texture(VkDescriptorSet descriptor_set) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  // Frames up to and including the one currently being recorded may still reference the old
  // descriptor set through in-flight ImGui draw data; only free entry once the GPU has caught up.
  const auto frame_index = graphics_module.frame_context().frame_index();

  _pending_texture_frees.emplace_back(descriptor_set, frame_index);
}

auto ui_system::_collect_pending_textures() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto completed_value = graphics_module.frame_context().timeline_value();

  std::erase_if(_pending_texture_frees, [completed_value](const auto& entry) {
    const auto& [descriptor_set, frame_index] = entry;

    if (frame_index > completed_value) {
      return false;
    }

    ImGui_ImplVulkan_RemoveTexture(descriptor_set);

    return true;
  });
}

} // namespace sbx::render
