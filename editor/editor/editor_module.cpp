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

#include <editor/panels/asset_browser_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/properties_panel.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/validate.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_packet.hpp>
#include <libsbx/render/present_pass.hpp>

namespace editor {

class viewport_composite_pass final : public sbx::render::render_pass {

public:

  viewport_composite_pass() {

  }

  ~viewport_composite_pass() override {
    
  }

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Editor";
  }

  auto execute(sbx::render::render_context& context) -> void override {
    auto* draw_data = ImGui::GetDrawData();

    if (draw_data == nullptr || !draw_data->Valid || draw_data->CmdListsCount == 0) {
      return;
    }

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();
    auto& swapchain = graphics_module.frame_context().swapchain();

    if (context.packet->camera.is_active) {
      auto& scene = registry.get<sbx::graphics::image>(context.scene);

      auto to_read = sbx::graphics::command_buffer::image_transition_data{};
      to_read.image = scene;
      to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      to_read.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      to_read.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
      to_read.old_layout = sbx::graphics::image_layout::color_attachment_optimal;
      to_read.new_layout = sbx::graphics::image_layout::shader_read_only_optimal;
      to_read.aspect_mask = scene.aspect();
      to_read.layer_count = 1u;
      context.command_buffer->transition_image_layout(to_read);
    }

    auto color_attachment = VkRenderingAttachmentInfo{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain.active_image_view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = VkClearColorValue{{0.1f, 0.1f, 0.1f, 1.0f}};

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.swapchain_extent.x(), context.swapchain_extent.y()}};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &color_attachment;

    context.command_buffer->begin_rendering(rendering_info);
    ImGui_ImplVulkan_RenderDrawData(draw_data, *context.command_buffer);
    context.command_buffer->end_rendering();
  }

private:

  sbx::math::vector2u _panel_size{0u, 0u};
  sbx::math::vector2 _content_min{0.0f, 0.0f};

}; // class viewport_composite_pass

static auto viewport_sampler_create_info() -> sbx::graphics::sampler::create_info {
  return sbx::graphics::sampler::create_info{
    .mag_filter = sbx::graphics::filter::linear,
    .min_filter = sbx::graphics::filter::linear,
    .mipmap_mode = sbx::graphics::mipmap_mode::linear,
    .address_mode_u = sbx::graphics::address_mode::clamp_to_edge,
    .address_mode_v = sbx::graphics::address_mode::clamp_to_edge,
    .address_mode_w = sbx::graphics::address_mode::clamp_to_edge,
    .max_anisotropy = 1.0f,
    .max_lod = VK_LOD_CLAMP_NONE,
    .name = "Editor Viewport Sampler"
  };
}

editor_module::editor_module()
: _ini_file{ini_file},
  _sampler{viewport_sampler_create_info()}  {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = _ini_file.data();

  _create_descriptor_pool();
  _initialize_backends();
  _upload_fonts();
  _apply_style();

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  render_module.set_composite_pass(std::make_unique<viewport_composite_pass>());
  render_module.set_pre_render_callback([this]() { _build_ui_frame(); });
}

editor_module::~editor_module() {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  // The device is fully idle here, so every pending free below is safe to flush unconditionally
  // rather than waiting on its retirement frame.
  for (auto& [descriptor_set, frame_index] : _pending_texture_frees) {
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);
  }

  _pending_texture_frees.clear();

  if (_texture_id != VK_NULL_HANDLE && ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplVulkan_RemoveTexture(_texture_id);
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(logical_device, _descriptor_pool, nullptr);
}

auto editor_module::_build_ui_frame() -> void {
  _collect_pending_textures();

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  _draw_dockspace();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // ImGui::PopStyleVar();

  _viewport_is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  const auto scene_image = render_module.scene_image();

  if (scene_image.is_valid() && available.x > 0.0f && available.y > 0.0f) {
    render_module.set_viewport_extent(sbx::math::vector2u{width, height});

    _update_texture(scene_image);

    if (_texture_id != VK_NULL_HANDLE) {
      ImGui::Image(reinterpret_cast<ImTextureID>(_texture_id), available);
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::Begin("Stats");
  ImGui::Text("%.1f FPS (%.3f ms)", static_cast<double>(ImGui::GetIO().Framerate), 1000.0 / static_cast<double>(ImGui::GetIO().Framerate));
  ImGui::End();

  draw_hierarchy_panel(_state);
  draw_properties_panel(_state);
  draw_asset_browser_panel(_state);

  ImGui::Render();
}

auto editor_module::_update_texture(sbx::graphics::image_handle image) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto current_view = registry.get<sbx::graphics::image>(image).view();

  if (current_view == _cached_view) {
    return;
  }

  if (_texture_id != VK_NULL_HANDLE) {
    _retire_texture(_texture_id);
  }

  _texture_id = ImGui_ImplVulkan_AddTexture(_sampler, current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  _cached_view = current_view;
}

auto editor_module::_retire_texture(VkDescriptorSet descriptor_set) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  // Frames up to and including the one currently being recorded may still reference the old
  // descriptor set through in-flight ImGui draw data; only free it once the GPU has caught up.
  const auto frame_index = graphics_module.frame_context().frame_index();

  _pending_texture_frees.emplace_back(descriptor_set, frame_index);
}

auto editor_module::_collect_pending_textures() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

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

auto editor_module::_initialize_backends() -> void {
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
