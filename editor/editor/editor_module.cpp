// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <memory>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <editor/backends/v1.92.9-docking/imgui_impl_glfw.h>
#include <editor/backends/v1.92.9-docking/imgui_impl_vulkan.h>

#include <editor/fonts/material_design_icons.hpp>

#include <editor/panels/asset_browser_panel.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/logger_panel.hpp>
#include <editor/panels/properties_panel.hpp>

#include <editor/viewport_gizmo.hpp>
#include <editor/viewport_picking.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/core/project.hpp>

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
  _create_panels();

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
  ImGuizmo::BeginFrame();

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

      // Captured before the gizmo call below, since ImGuizmo's own widgets can disturb ImGui's
      // "last item" tracking that IsItemClicked/GetItemRectMin rely on.
      const auto image_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const auto image_origin = ImGui::GetItemRectMin();

      const auto gizmo_active = draw_viewport_gizmo(_state, image_origin, available);

      // Left-click picks the node under the cursor, unless it landed on the gizmo (right-drag is
      // already the fly camera, so there's no input conflict there either).
      if (image_clicked && !gizmo_active) {
        const auto mouse_position = ImGui::GetMousePos();

        pick_node_at_viewport_position(_state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::Begin("Stats");
  ImGui::Text("%.1f FPS (%.3f ms)", static_cast<double>(ImGui::GetIO().Framerate), 1000.0 / static_cast<double>(ImGui::GetIO().Framerate));
  ImGui::End();

  for (auto& panel : _panels) {
    panel->draw(_state);
  }

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
  auto& style = ImGui::GetStyle();
  auto* colors = style.Colors;

  // Catppuccin Mocha Palette
  // --------------------------------------------------------
  const auto base       = ImVec4(0.117f, 0.117f, 0.172f, 1.0f); // #1e1e2e
  const auto mantle     = ImVec4(0.109f, 0.109f, 0.156f, 1.0f); // #181825
  const auto surface0   = ImVec4(0.200f, 0.207f, 0.286f, 1.0f); // #313244
  const auto surface1   = ImVec4(0.247f, 0.254f, 0.337f, 1.0f); // #3f4056
  const auto surface2   = ImVec4(0.290f, 0.301f, 0.388f, 1.0f); // #4a4d63
  const auto overlay0   = ImVec4(0.396f, 0.403f, 0.486f, 1.0f); // #65677c
  const auto overlay2   = ImVec4(0.576f, 0.584f, 0.654f, 1.0f); // #9399b2
  const auto text       = ImVec4(0.803f, 0.815f, 0.878f, 1.0f); // #cdd6f4
  const auto subtext0   = ImVec4(0.639f, 0.658f, 0.764f, 1.0f); // #a3a8c3
  const auto mauve      = ImVec4(0.796f, 0.698f, 0.972f, 1.0f); // #cba6f7
  const auto peach      = ImVec4(0.980f, 0.709f, 0.572f, 1.0f); // #fab387
  const auto yellow     = ImVec4(0.980f, 0.913f, 0.596f, 1.0f); // #f9e2af
  const auto green      = ImVec4(0.650f, 0.890f, 0.631f, 1.0f); // #a6e3a1
  const auto teal       = ImVec4(0.580f, 0.886f, 0.819f, 1.0f); // #94e2d5
  const auto sapphire   = ImVec4(0.458f, 0.784f, 0.878f, 1.0f); // #74c7ec
  const auto blue       = ImVec4(0.533f, 0.698f, 0.976f, 1.0f); // #89b4fa
  const auto lavender   = ImVec4(0.709f, 0.764f, 0.980f, 1.0f); // #b4befe

  // Main window and backgrounds
  colors[ImGuiCol_WindowBg]             = base;
  colors[ImGuiCol_ChildBg]              = base;
  colors[ImGuiCol_PopupBg]              = surface0;
  colors[ImGuiCol_Border]               = surface1;
  colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg]              = surface0;
  colors[ImGuiCol_FrameBgHovered]       = surface1;
  colors[ImGuiCol_FrameBgActive]        = surface2;
  colors[ImGuiCol_TitleBg]              = mantle;
  colors[ImGuiCol_TitleBgActive]        = surface0;
  colors[ImGuiCol_TitleBgCollapsed]     = mantle;
  colors[ImGuiCol_MenuBarBg]            = mantle;
  colors[ImGuiCol_ScrollbarBg]          = surface0;
  colors[ImGuiCol_ScrollbarGrab]        = surface2;
  colors[ImGuiCol_ScrollbarGrabHovered] = overlay0;
  colors[ImGuiCol_ScrollbarGrabActive]  = overlay2;
  colors[ImGuiCol_CheckMark]            = text;
  colors[ImGuiCol_SliderGrab]           = sapphire;
  colors[ImGuiCol_SliderGrabActive]     = blue;
  colors[ImGuiCol_Button]               = surface0;
  colors[ImGuiCol_ButtonHovered]        = surface1;
  colors[ImGuiCol_ButtonActive]         = surface2;
  colors[ImGuiCol_Header]               = surface0;
  colors[ImGuiCol_HeaderHovered]        = surface1;
  colors[ImGuiCol_HeaderActive]         = surface2;
  colors[ImGuiCol_Separator]            = surface1;
  colors[ImGuiCol_SeparatorHovered]     = mauve;
  colors[ImGuiCol_SeparatorActive]      = mauve;
  colors[ImGuiCol_ResizeGrip]           = surface2;
  colors[ImGuiCol_ResizeGripHovered]    = mauve;
  colors[ImGuiCol_ResizeGripActive]     = mauve;
  colors[ImGuiCol_Tab]                  = surface0;
  colors[ImGuiCol_TabHovered]           = surface2;
  colors[ImGuiCol_TabActive]            = surface1;
  colors[ImGuiCol_TabUnfocused]         = surface0;
  colors[ImGuiCol_TabUnfocusedActive]   = surface1;
  colors[ImGuiCol_DockingPreview]       = sapphire;
  colors[ImGuiCol_DockingEmptyBg]       = base;
  colors[ImGuiCol_PlotLines]            = blue;
  colors[ImGuiCol_PlotLinesHovered]     = peach;
  colors[ImGuiCol_PlotHistogram]        = teal;
  colors[ImGuiCol_PlotHistogramHovered] = green;
  colors[ImGuiCol_TableHeaderBg]        = surface0;
  colors[ImGuiCol_TableBorderStrong]    = surface1;
  colors[ImGuiCol_TableBorderLight]     = surface0;
  colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
  colors[ImGuiCol_TextSelectedBg]       = surface2;
  colors[ImGuiCol_DragDropTarget]       = yellow;
  colors[ImGuiCol_NavHighlight]         = lavender;
  colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
  colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
  colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
  colors[ImGuiCol_Text]                 = text;
  colors[ImGuiCol_TextDisabled]         = subtext0;

  // Rounded corners
  style.WindowRounding    = 6.0f;
  style.ChildRounding     = 6.0f;
  style.FrameRounding     = 4.0f;
  style.PopupRounding     = 4.0f;
  style.ScrollbarRounding = 9.0f;
  style.GrabRounding      = 4.0f;
  style.TabRounding       = 4.0f;

  // Padding and spacing
  style.WindowPadding     = ImVec2(8.0f, 8.0f);
  style.FramePadding      = ImVec2(5.0f, 3.0f);
  style.ItemSpacing       = ImVec2(8.0f, 4.0f);
  style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
  style.IndentSpacing     = 21.0f;
  style.ScrollbarSize     = 14.0f;
  style.GrabMinSize       = 10.0f;

  // Borders
  style.WindowBorderSize  = 1.0f;
  style.ChildBorderSize   = 1.0f;
  style.PopupBorderSize   = 1.0f;
  style.FrameBorderSize   = 0.0f;
  style.TabBorderSize     = 0.0f;

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

auto editor_module::_create_panels() -> void {
  _panels.push_back(std::make_unique<hierarchy_panel>());
  _panels.push_back(std::make_unique<properties_panel>());
  _panels.push_back(std::make_unique<asset_browser_panel>());
  _panels.push_back(std::make_unique<logger_panel>());
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
        request_quit();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
      if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto& scene = scenes_module.active_scene();

        _state.select_node(scene.create_node());
      }

      ImGui::Separator();

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save")) {
        if (_scene_path.empty()) {
          std::strncpy(_save_as_buffer.data(), "scenes/new_scene.yaml", _save_as_buffer.size() - 1u);
          _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
          _show_save_as_dialog = true;
        } else {
          _save_scene(_scene_path);
        }
      }

      if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE_EDIT " Save As...")) {
        const auto& seed = _scene_path.empty() ? std::string{"scenes/new_scene.yaml"} : _scene_path.string();
        std::strncpy(_save_as_buffer.data(), seed.c_str(), _save_as_buffer.size() - 1u);
        _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
        _show_save_as_dialog = true;
      }

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  _draw_save_as_dialog();
  _draw_unsaved_changes_dialog();

  ImGui::End();
}

auto editor_module::request_quit() -> void {
  if (_is_scene_dirty()) {
    _show_unsaved_changes_dialog = true;
  } else {
    sbx::core::engine::quit();
  }
}

auto editor_module::_save_scene(const std::filesystem::path& path) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  sbx::scenes::scene_serializer::save(scenes_module.active_scene(), path);

  _scene_path = path;
}

auto editor_module::_is_scene_dirty() -> bool {
  if (_scene_path.empty()) {
    return true; // never saved — anything at all counts as unsaved
  }

  auto& project = sbx::core::engine::project();
  auto file = std::ifstream{project.assets_directory() / _scene_path, std::ios::binary};

  if (!file) {
    return true; // no file at that path (yet)
  }

  const auto on_disk = std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  return on_disk != sbx::scenes::scene_serializer::serialize(scenes_module.active_scene());
}

auto editor_module::_draw_save_as_dialog() -> void {
  if (_show_save_as_dialog) {
    ImGui::OpenPopup("Save Scene As");
    _show_save_as_dialog = false;
  }

  if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextDisabled("Relative to the project's assets directory.");
    ImGui::InputText("##save_as_path", _save_as_buffer.data(), _save_as_buffer.size());

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (_save_as_buffer[0] != '\0') {
        _save_scene(std::filesystem::path{_save_as_buffer.data()});

        if (_quit_after_save_as) {
          _quit_after_save_as = false;
          sbx::core::engine::quit();
        }

        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

auto editor_module::_draw_unsaved_changes_dialog() -> void {
  if (_show_unsaved_changes_dialog) {
    ImGui::OpenPopup("Unsaved Changes");
    _show_unsaved_changes_dialog = false;
  }

  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(ICON_MDI_CONTENT_SAVE_ALERT " The current scene has unsaved changes.");

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (_scene_path.empty()) {
        std::strncpy(_save_as_buffer.data(), "scenes/new_scene.yaml", _save_as_buffer.size() - 1u);
        _save_as_buffer[_save_as_buffer.size() - 1u] = '\0';
        _show_save_as_dialog = true;
        _quit_after_save_as = true;
      } else {
        _save_scene(_scene_path);
        sbx::core::engine::quit();
      }

      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Don't Save")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
      sbx::core::engine::quit();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
