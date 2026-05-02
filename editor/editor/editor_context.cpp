// SPDX-License-Identifier: MIT
#include <editor/editor_context.hpp>

#include <cstring>

#include <editor/bindings/imgui.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/devices/devices_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

editor_context::editor_context() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  _ini_file = assets_module.resolve_path(ini_file).string();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = _ini_file.data();

  _create_descriptor_pool();
  _init_backends();
  _upload_fonts();
  _apply_style();
}

editor_context::~editor_context() {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(graphics_module.logical_device(), _descriptor_pool, nullptr);
  }
}

auto editor_context::new_frame() -> void {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
}

auto editor_context::render() -> void {
  ImGui::Render();
}

auto editor_context::render_draw_data(VkCommandBuffer command_buffer) -> void {
  auto* draw_data = ImGui::GetDrawData();

  if (draw_data != nullptr) {
    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
  }
}

auto editor_context::wants_capture_mouse() const -> bool {
  return ImGui::GetIO().WantCaptureMouse;
}

auto editor_context::wants_capture_keyboard() const -> bool {
  return ImGui::GetIO().WantCaptureKeyboard;
}

auto editor_context::_create_descriptor_pool() -> void {
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

  sbx::graphics::validate(vkCreateDescriptorPool(graphics_module.logical_device(), &pool_info, nullptr, &_descriptor_pool));
}

auto editor_context::_init_backends() -> void {
  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  ImGui_ImplGlfw_InitForVulkan(devices_module.window().handle(), true);

  auto& surface = graphics_module.surface();
  auto surface_capabilities = surface.capabilities();

  auto image_count = surface_capabilities.minImageCount + 1u;

  if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  auto surface_format = surface.format().format;

  auto init_info = ImGui_ImplVulkan_InitInfo{};
  std::memset(&init_info, 0, sizeof(ImGui_ImplVulkan_InitInfo));
  init_info.Instance = graphics_module.instance();
  init_info.PhysicalDevice = graphics_module.physical_device();
  init_info.Device = graphics_module.logical_device();
  init_info.QueueFamily = graphics_module.logical_device().queue<sbx::graphics::queue::type::graphics>().family();
  init_info.Queue = graphics_module.logical_device().queue<sbx::graphics::queue::type::graphics>();
  init_info.DescriptorPool = _descriptor_pool;
  init_info.MinImageCount = sbx::graphics::swapchain::max_frames_in_flight;
  init_info.ImageCount = image_count;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &surface_format;
  init_info.MinAllocationSize = 1024 * 1024;

  ImGui_ImplVulkan_Init(&init_info);
}

auto editor_context::_upload_fonts() -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto& io = ImGui::GetIO();

  const auto resolved_font_path = assets_module.resolve_path(font_path);

  auto* font = io.Fonts->AddFontFromFileTTF(resolved_font_path.string().c_str(), 16.0f);

  io.FontDefault = font;

  const auto resolved_icon_path = assets_module.resolve_path(icon_path);

  static constexpr auto icon_ranges = std::array<ImWchar, 3>{ICON_MIN_MDI, ICON_MAX_MDI, 0};

  auto icon_config = ImFontConfig{};
  icon_config.MergeMode = true;
  icon_config.PixelSnapH = true;
  icon_config.GlyphMinAdvanceX = 16.0f;
  icon_config.GlyphOffset.y = 1.0f;

  io.Fonts->AddFontFromFileTTF(resolved_icon_path.string().c_str(), 16.0f, &icon_config, icon_ranges.data());

  io.Fonts->Build();

  ImGui_ImplVulkan_CreateFontsTexture();
}

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

static auto lerp(const ImVec4& a, const ImVec4& b, float t) -> ImVec4 {
  return ImVec4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

auto editor_context::_apply_style() -> void {
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
  style.TabMinWidthForCloseButton = 0.0f;
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

  auto color_accent_2 = lerp(h_color_1, h_color_2, 0.2f);
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

} // namespace editor
