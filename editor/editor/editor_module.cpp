// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <array>
#include <memory>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <editor/backends/v1.92.9-docking/imgui_impl_glfw.h>
#include <editor/backends/v1.92.9-docking/imgui_impl_vulkan.h>

#include <editor/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/validate.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_packet.hpp>
#include <libsbx/render/present_pass.hpp>

namespace editor {

// Deep copy of the finished ImGui draw data. A fresh one per frame rides the render packet, so the
// main thread's next NewFrame cannot clobber what the render thread is still recording.
struct imgui_frame final : sbx::render::render_packet_extension {

  ImDrawData data{};
  ImVector<ImDrawList*> lists{};

  ~imgui_frame() override {
    for (auto* list : lists) {
      IM_DELETE(list);
    }
  }
}; // struct imgui_frame

auto capture_imgui_frame() -> std::unique_ptr<sbx::render::render_packet_extension> {
  const auto* source = ImGui::GetDrawData();

  if (source == nullptr || !source->Valid || source->CmdListsCount == 0) {
    return nullptr;
  }

  auto frame = std::make_unique<imgui_frame>();

  frame->data = *source;

  frame->lists.reserve(source->CmdListsCount);

  for (auto index = 0; index < source->CmdListsCount; ++index) {
    frame->lists.push_back(source->CmdLists[index]->CloneOutput());
  }

  frame->data.CmdLists = frame->lists;

  return frame;
}

// Composite: blit the scene image to the swapchain (default present), then draw ImGui over it.
class viewport_composite_pass final : public sbx::render::render_pass {

public:

  ~viewport_composite_pass() override {
    if (_texture_id != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr) {
      ImGui_ImplVulkan_RemoveTexture(_texture_id);
    }
  }

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Editor";
  }

  auto execute(sbx::render::render_context& context) -> void override {
    if (!context.packet->camera.is_active) {
      return;
    }

    _present.execute(context);

    const auto* frame = dynamic_cast<const imgui_frame*>(context.extension.get());

    if (frame == nullptr) {
      return;
    }

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    auto& frame_context = graphics_module.frame_context();
    auto& swapchain = frame_context.swapchain();

    auto barrier = VkMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    context.command_buffer->memory_dependency(barrier);

    auto color_attachment = VkRenderingAttachmentInfo{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain.active_image_view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &color_attachment;

    context.command_buffer->begin_rendering(rendering_info);
    ImGui_ImplVulkan_RenderDrawData(const_cast<ImDrawData*>(&frame->data), *context.command_buffer);
    context.command_buffer->end_rendering();
  }

private:

  auto _update_texture(const sbx::graphics::image_handle& image) -> void {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& registry = graphics_module.resource_registry();

    auto& scene = registry.get<sbx::graphics::image>(image);

    auto current_view = scene.view();

    if (current_view == _cached_view) {
      return;
    }

    if (_texture_id != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(_texture_id);
    }

    _texture_id = ImGui_ImplVulkan_AddTexture(image.sampler(), current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _cached_view = current_view;
  }

  sbx::render::present_pass _present{};

  VkDescriptorSet _texture_id{VK_NULL_HANDLE};
  VkImageView _cached_view{VK_NULL_HANDLE};

  sbx::math::vector2u _panel_size{0u, 0u};
  sbx::math::vector2 _content_min{0.0f, 0.0f};

}; // class viewport_composite_pass

editor_module::editor_module()
: _ini_file{ini_file} {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = _ini_file.data();

  _create_descriptor_pool();
  _init_backends();
  _upload_fonts();
  _apply_style();

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  render_module.set_packet_producer([]() { return capture_imgui_frame(); });
  render_module.set_composite_pass(std::make_unique<viewport_composite_pass>());
}

editor_module::~editor_module() {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (_descriptor_pool) {
    vkDestroyDescriptorPool(graphics_module.logical_device(), _descriptor_pool, nullptr);
  }
}

auto editor_module::post_update() -> void {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  _draw_dockspace();

  ImGui::Begin(ICON_MDI_BUG_OUTLINE " Debug");
  ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
  ImGui::End();

  ImGui::Render();
}

auto editor_module::_create_descriptor_pool() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto pool_sizes = std::array<VkDescriptorPoolSize, 1>{{
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64}
  }};

  auto pool_info = VkDescriptorPoolCreateInfo{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 64;
  pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  sbx::graphics::validate(vkCreateDescriptorPool(graphics_module.logical_device(), &pool_info, nullptr, &_descriptor_pool), "vkCreateDescriptorPool");
}

auto editor_module::_init_backends() -> void {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& window = platform_module.window();

  ImGui_ImplGlfw_InitForVulkan(window, true);

  auto& surface = graphics_module.surface();
  auto& logical_device = graphics_module.logical_device();
  auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();

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
  init_info.MinImageCount = sbx::graphics::swapchain::max_frames_in_flight;
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

auto editor_module::_upload_fonts() -> void {
  auto& io = ImGui::GetIO();

  const auto resolved_font_path = std::string{font_path};

  auto* font = io.Fonts->AddFontFromFileTTF(resolved_font_path.c_str(), 16.0f);

  io.FontDefault = font;

  const auto resolved_icon_path = std::string{icon_path};

  static constexpr auto icon_ranges = std::array<ImWchar, 3>{ICON_MIN_MDI, ICON_MAX_MDI, 0};

  auto icon_config = ImFontConfig{};
  icon_config.MergeMode = true;
  icon_config.PixelSnapH = true;
  icon_config.GlyphMinAdvanceX = 16.0f;
  icon_config.GlyphOffset.y = 1.0f;

  io.Fonts->AddFontFromFileTTF(resolved_icon_path.c_str(), 16.0f, &icon_config, icon_ranges.data());
}

auto editor_module::_apply_style() -> void {
  static auto bg_color_1 = ImVec4{0.1f,0.1f,0.1f,1.0f};
  static auto bg_color_2 = ImVec4{0.59f,0.59f,0.59f,1.0f};
  static auto h_color_1 = ImVec4{1.0f,1.0f,1.0f,1.0f};
  static auto h_color_2 = ImVec4{1.0f,1.0f,1.0f,0.1f};
  static auto color_accent_1 = ImVec4{59.0f / 255.0f, 79.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  static auto color_accent_2 = ImVec4{45.0f / 255.0f, 80.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  static auto color_ok = ImVec4{51.0f / 255.0f, 179.0f / 255.0f, 89.0f / 255.0f, 1.0f};
  static auto color_info = ImVec4{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
  static auto color_warning = ImVec4{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
  static auto color_error = ImVec4{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

  auto lerp = [](const ImVec4& a, const ImVec4& b, std::float_t t) -> ImVec4 {
    return ImVec4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
  };

  ImGui::StyleColorsDark();

  auto& style = ImGui::GetStyle();

  bg_color_1 = ImVec4{30.0f / 255.0f, 30.0f / 255.0f, 41.0f / 255.0f, 1.0f};
  bg_color_2 = ImVec4{71.0f / 255.0f, 85.0f / 255.0f, 117.0f / 255.0f, 1.0f};

  h_color_1 = ImVec4{1.0, 1.0, 1.0, 1.0f};
  h_color_2 = ImVec4{1.0, 1.0, 1.0, 0.1f};

  color_accent_1 = ImVec4{181.0f / 255.0f, 198.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  color_accent_2 = ImVec4{79.0f / 255.0f, 82.0f / 255.0f, 99.0f / 255.0f, 1.0f};

  color_ok = ImVec4{51.0f / 255.0f, 179.0f / 255.0f, 89.0f / 255.0f, 1.0f};
  color_info = ImVec4{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
  color_warning = ImVec4{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
  color_error = ImVec4{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.60f;

  style.WindowPadding = ImVec2(8.0f, 4.0f);
  style.CellPadding = ImVec2(8.0f, 4.0f);
  style.FramePadding = ImVec2(8.0f, 4.0f);
  style.ItemSpacing = ImVec2(8.0f, 4.0f);

  style.WindowRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.TabRounding = 2.0f;
  style.ChildRounding = 2.0f;
  style.PopupRounding = 2.0f;
  style.FrameRounding = 2.0f;
  style.ScrollbarRounding = 2.0f;

  style.WindowBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;

  style.ChildBorderSize = 0.0f;
  style.FrameBorderSize = 0.0f;
  style.TabBorderSize = 0.0f;

  style.WindowMinSize = ImVec2(32.0f, 32.0f);
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_Left;

  style.ItemInnerSpacing = ImVec2(2.0f, 2.0f);
  style.IndentSpacing = 21.0f;
  style.ColumnsMinSpacing = 6.0f;
  style.ScrollbarSize = 13.0f;
  style.GrabMinSize = 7.0f;
  style.ColorButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

  auto color_background_1 = lerp(bg_color_1, bg_color_2, .0f);
  auto color_background_2 = lerp(bg_color_1, bg_color_2, .1f);
  auto color_background_3 = lerp(bg_color_1, bg_color_2, .2f);
  auto color_background_4 = lerp(bg_color_1, bg_color_2, .3f);
  auto color_background_5 = lerp(bg_color_1, bg_color_2, .4f);
  auto color_background_6 = lerp(bg_color_1, bg_color_2, .5f);
  auto color_background_7 = lerp(bg_color_1, bg_color_2, .6f);
  auto color_background_8 = lerp(bg_color_1, bg_color_2, .7f);
  auto color_background_9 = lerp(bg_color_1, bg_color_2, .8f);
  auto color_background_10 = lerp(bg_color_1, bg_color_2, .9f);

  // should be dark
  auto color_black_transparent_9 = ImVec4{0.0f, 0.0f, 0.0f, 0.9f};
  auto color_black_transparent_6 = ImVec4{0.0f, 0.0f, 0.0f, 0.6f};
  auto color_black_transparent_3 = ImVec4{0.0f, 0.0f, 0.0f, 0.3f};
  auto color_black_transparent_1 = ImVec4{0.0f, 0.0f, 0.0f, 0.1f};

  auto color_highlight_1 = lerp(h_color_1, h_color_2, 0);

  // auto color_accent_2 = lerp(h_color_1, h_color_2, 0.2f);
  auto color_accent_3 = lerp(h_color_1, h_color_2, 0.3f);

  style.Colors[ImGuiCol_Text] = color_highlight_1;
  style.Colors[ImGuiCol_TextDisabled] = color_background_9;

  style.Colors[ImGuiCol_WindowBg] = color_background_2;
  style.Colors[ImGuiCol_FrameBg] = color_background_4;
  style.Colors[ImGuiCol_TitleBg] = color_background_1;
  style.Colors[ImGuiCol_TitleBgActive] = color_background_2;

  // accent
  style.Colors[ImGuiCol_ScrollbarGrabActive] = color_accent_1;
  style.Colors[ImGuiCol_SeparatorActive] = color_accent_1;
  style.Colors[ImGuiCol_SliderGrabActive] = color_accent_1;
  style.Colors[ImGuiCol_ResizeGripActive] = color_accent_1;
  style.Colors[ImGuiCol_DragDropTarget] = color_accent_1;
  style.Colors[ImGuiCol_NavCursor] = color_accent_1;
  style.Colors[ImGuiCol_NavWindowingHighlight] = color_accent_1;
  style.Colors[ImGuiCol_TabSelectedOverline] = color_accent_1;
  style.Colors[ImGuiCol_TabDimmedSelectedOverline] = color_accent_1;
  style.Colors[ImGuiCol_CheckMark] = color_accent_1;

  style.Colors[ImGuiCol_Tab] = color_background_1;
  style.Colors[ImGuiCol_TabDimmed] = color_background_1;
  style.Colors[ImGuiCol_TabHovered] = color_background_4;
  style.Colors[ImGuiCol_TabSelected] = lerp(color_background_6, color_accent_1, 0.25f);
  style.Colors[ImGuiCol_TabDimmedSelected] = color_background_4;

  style.Colors[ImGuiCol_FrameBgHovered] = color_background_3;

  style.Colors[ImGuiCol_TitleBgCollapsed] = color_background_2;
  style.Colors[ImGuiCol_MenuBarBg] = color_background_3;
  style.Colors[ImGuiCol_ScrollbarBg] = color_background_2;

  style.Colors[ImGuiCol_Button] = color_background_3;
  style.Colors[ImGuiCol_ButtonHovered] = color_background_4;
  style.Colors[ImGuiCol_ButtonActive] = color_background_1;

  style.Colors[ImGuiCol_ResizeGrip] = color_black_transparent_3;
  style.Colors[ImGuiCol_ResizeGripHovered] = color_black_transparent_6;
  style.Colors[ImGuiCol_TableRowBgAlt] = color_black_transparent_1;
  style.Colors[ImGuiCol_TextSelectedBg] = color_black_transparent_1;

  style.Colors[ImGuiCol_DockingPreview] = color_accent_1;
  style.Colors[ImGuiCol_PlotLinesHovered] = color_accent_2;
  style.Colors[ImGuiCol_PlotHistogramHovered] = color_accent_3;

  style.Colors[ImGuiCol_PlotHistogram] = color_background_10;

  style.Colors[ImGuiCol_HeaderHovered] = color_background_9;
  style.Colors[ImGuiCol_HeaderActive] = color_background_9;
  style.Colors[ImGuiCol_PlotLines] = color_background_9;

  style.Colors[ImGuiCol_SeparatorHovered] = color_background_8;
  style.Colors[ImGuiCol_SliderGrab] = color_background_8;
  style.Colors[ImGuiCol_PopupBg] = color_background_6;
  style.Colors[ImGuiCol_Header] = color_background_6;
  style.Colors[ImGuiCol_TableBorderStrong] = color_background_6;
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = color_background_6;
  style.Colors[ImGuiCol_Separator] = color_background_4;
  style.Colors[ImGuiCol_TableBorderLight] = color_background_4;
  style.Colors[ImGuiCol_FrameBgActive] = color_background_5;
  style.Colors[ImGuiCol_ScrollbarGrab] = color_background_5;

  style.Colors[ImGuiCol_ChildBg] = {};
  style.Colors[ImGuiCol_Border] = color_background_5;

  style.Colors[ImGuiCol_TableHeaderBg] = color_background_3;

  style.Colors[ImGuiCol_NavWindowingDimBg] = color_black_transparent_6;
  style.Colors[ImGuiCol_ModalWindowDimBg] = color_black_transparent_6;

  style.Colors[ImGuiCol_TableRowBg] = {};
  style.Colors[ImGuiCol_BorderShadow] = {};

  // Go through every colour and convert it to linear
  // This is because ImGui uses linear colours but we are using sRGB
  // This is a simple approximation of the conversion
  for (auto i = 0; i < ImGuiCol_COUNT; ++i) {
    auto& color = style.Colors[i];
    color.x = color.x <= 0.04045f ? color.x / 12.92f : std::pow((color.x + 0.055f) / 1.055f, 2.4f);
    color.y = color.y <= 0.04045f ? color.y / 12.92f : std::pow((color.y + 0.055f) / 1.055f, 2.4f);
    color.z = color.z <= 0.04045f ? color.z / 12.92f : std::pow((color.z + 0.055f) / 1.055f, 2.4f);
  }
}

auto editor_module::_draw_dockspace() -> void {
  auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

  auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

  ImGui::Begin("##dockspace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGui::DockSpace(ImGui::GetID("editor_dockspace"), ImVec2{0.0f, 0.0f});

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Quit")) {
        sbx::core::engine::quit();
      }

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();
}

} // namespace editor
