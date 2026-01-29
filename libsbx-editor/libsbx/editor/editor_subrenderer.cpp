// SPDX-License-Identifier: MIT
#include <libsbx/editor/editor_subrenderer.hpp>

namespace sbx::editor {

editor_subrenderer::editor_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name)
: base{},
  _attachment_name{attachment_name},
  _pipeline{path, attachments},
  _descriptor_handler{_pipeline, 0u},
  _editor_theme{} {
  // Initialize ImGui
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImNodes::CreateContext();
  ImPlot::CreateContext();

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  _actual_ini_file = assets_module.resolve_path(ini_file).string();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = _actual_ini_file.data();

  // ImGui::StyleColorsDark();
  ImNodes::StyleColorsDark();

  // _setup_style();

  _editor_theme.apply_theme("Bar");

  _editor_font.load_font("Roboto", "res://fonts/Roboto-Regular.ttf", 16.0f);
  _editor_font.load_font("Geist", "res://fonts/Geist-Regular.ttf", 16.0f);
  _editor_font.load_font("JetBrainsMono", "res://fonts/JetBrainsMono-Medium.ttf", 16.0f);

  _editor_font.set_active_font("Roboto");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  // Project Menu

  auto project_menu_new = editor::menu_item{};
  project_menu_new.title = "New...";
  project_menu_new.separator_after = true;
  project_menu_new.on_click = [this]() { utility::logger<"editor">::debug("Project::New clicked"); };

  auto project_menu_open = editor::menu_item{};
  project_menu_open.title = "Open...";
  project_menu_open.short_cut = "Ctrl+O";
  project_menu_open.separator_after = true;
  project_menu_open.on_click = [this]() { utility::logger<"editor">::debug("Project::Open clicked"); };

  auto project_menu_save = editor::menu_item{};
  project_menu_save.title = "Save";
  project_menu_save.short_cut = "Ctrl+S";
  project_menu_save.on_click = [this, &scenes_module]() { 
    utility::logger<"editor">::debug("Project::Save clicked");
    
    scenes_module.save_scene("res://scenes/scene.yaml");
  };

  auto project_menu_save_as = editor::menu_item{};
  project_menu_save_as.title = "Save As...";
  project_menu_save_as.separator_after = true;
  project_menu_save_as.on_click = [this]() { utility::logger<"editor">::debug("Project::SaveAs clicked"); };

  auto project_menu_preferences = editor::menu_item{};
  project_menu_preferences.title = "Preferences";
  project_menu_preferences.on_click = [this]() { _open_popups.set(popup::menu_preferences); };

  auto project_menu = editor::menu{};
  project_menu.title = "Project";
  project_menu.items.push_back(project_menu_new);
  project_menu.items.push_back(project_menu_open);
  // project_menu.items.push_back(project_menu_save);
  // project_menu.items.push_back(project_menu_save_as);
  project_menu.items.push_back(project_menu_preferences);
  _menu.push_back(project_menu);
  
  // Scene Menu

  auto scene_menu_save = editor::menu_item{};
  scene_menu_save.title = "Save";
  scene_menu_save.short_cut = "Ctrl+S";
  scene_menu_save.on_click = [this, &scenes_module]() { 
    utility::logger<"editor">::debug("Scene::Save clicked");
    
    scenes_module.save_scene("res://scenes/scene.yaml");
  };

  auto scene_menu_save_as = editor::menu_item{};
  scene_menu_save_as.title = "Save As...";
  scene_menu_save_as.separator_after = true;
  scene_menu_save_as.on_click = [this]() { utility::logger<"editor">::debug("Scene::SaveAs clicked"); };

  auto scene_menu = editor::menu{};
  scene_menu.title = "Scene";
  scene_menu.items.push_back(scene_menu_save);
  scene_menu.items.push_back(scene_menu_save_as);
  _menu.push_back(scene_menu);

  // Help Menu

  auto help_menu_about = editor::menu_item{};
  help_menu_about.title = "About";
  help_menu_about.on_click = [this]() { _open_popups.set(popup::menu_about); };

  auto help_menu = editor::menu{};
  help_menu.title = "Help";
  help_menu.items.push_back(help_menu_about);
  _menu.push_back(help_menu);

  auto& device_module = sbx::core::engine::get_module<sbx::devices::devices_module>();
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_dynamic_size_callback([&]() -> sbx::math::vector2u {
    return _viewport_size;
  });

  devices::input::set_mouse_position_callback([&]() -> math::vector2 {
    return _mouse_position;
  });

  // Connect ImGui to GLFW
  ImGui_ImplGlfw_InitForVulkan(device_module.window(), true);

  auto& surface = graphics_module.surface();

  const auto surface_capabilities = surface.capabilities();

  auto desired_image_count = surface_capabilities.minImageCount + 3u;

  if (surface_capabilities.maxImageCount > 0 && desired_image_count > surface_capabilities.maxImageCount) {
    desired_image_count = surface_capabilities.maxImageCount;
  }

  // Connect ImGui to Vulkan
  auto init_info = ImGui_ImplVulkan_InitInfo{};
  std::memset(&init_info, 0, sizeof(ImGui_ImplVulkan_InitInfo));
  init_info.Instance = graphics_module.instance();
  init_info.PhysicalDevice = graphics_module.physical_device();
  init_info.Device = graphics_module.logical_device();
  init_info.QueueFamily = graphics_module.logical_device().queue<sbx::graphics::queue::type::graphics>().family();
  init_info.Queue = graphics_module.logical_device().queue<sbx::graphics::queue::type::graphics>();
  init_info.DescriptorPool = _pipeline.descriptor_pool();
  init_info.Subpass = 0;
  init_info.MinImageCount = 3u;
  init_info.ImageCount = desired_image_count;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo = _pipeline.rendering_info().info;
  // init_info.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_SRGB;

  ImGui_ImplVulkan_Init(&init_info);

  // Upload fonts
  ImGui_ImplVulkan_CreateFontsTexture();
}

editor_subrenderer::~editor_subrenderer() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImPlot::DestroyContext();
  ImNodes::DestroyContext();
  ImGui::DestroyContext();
}

auto editor_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("editor_subrenderer::render");
  
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("sTexture", graphics_module.attachment(_attachment_name));

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();

  _setup_dockspace();
  _setup_windows();

  ImGui::Render();
  auto* draw_data = ImGui::GetDrawData();

  ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
}

auto editor_subrenderer::_setup_dockspace() -> void {
  const auto* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  const auto window_flags = ImGuiWindowFlags{
    ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoBringToFrontOnFocus
  };

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("DockSpaceWithMenuBar", nullptr, window_flags);
  ImGui::PopStyleVar(2);

  add_menu_bar(_menu);

  // Create the dock space
  const auto dockspace_id = ImGui::GetID("DockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

  ImGui::End();
}

auto editor_subrenderer::_setup_windows() -> void {
  {
    const auto custom_backdrop_color = ImVec4{0.2f, 0.2f, 0.2f, 0.5f};

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, custom_backdrop_color);

    if (_open_popups.has(popup::menu_preferences)) {
      _open_popups.clear(popup::menu_preferences);
      ImGui::OpenPopup("Preferences");
    }
    
    ImGui::SetNextWindowSizeConstraints(ImVec2{600, 400}, ImVec2{800, 600});

    if (ImGui::BeginPopupModal("Preferences", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      static const auto categories = std::array<std::string_view, 4u>{ "General", "Display", "Audio", "Controls" };
      static auto selected_category = std::uint32_t{0u};

      const auto sidebar_width = 150.0f;

      if (ImGui::BeginChild("CategorySidebar", ImVec2(sidebar_width, 0), ImGuiChildFlags_Border)) {
        for (auto i = 0u; i < categories.size(); ++i) {
          if (ImGui::Selectable(categories[i].data(), selected_category == i)) {
            selected_category = i;
          }
        }
        ImGui::EndChild();
      }

      ImGui::SameLine();

      if (ImGui::BeginChild("SettingsContent", ImVec2(0, 0), ImGuiChildFlags_None)) {
        switch (selected_category) {
          case 0: {
            const auto available_fonts = _editor_font.get_fonts();
            const auto& active_font = _editor_font.get_active_font();

            ImGui::Text("Font");
            if (ImGui::BeginCombo("##FontSelector", active_font.c_str())) {
              for (const auto& font : available_fonts) {
                const auto is_selected = (font == active_font);

                if (ImGui::Selectable(font.c_str(), is_selected)) {
                  _editor_font.set_active_font(font);
                }

                if (is_selected) {
                  ImGui::SetItemDefaultFocus();
                }
              }
              
              ImGui::EndCombo();
            }

            ImGui::Separator();

            const auto available_themes = _editor_theme.get_themes();
            const auto& active_theme = _editor_theme.get_active_theme();

            ImGui::Text("Theme");
            if (ImGui::BeginCombo("##ThemeSelector", active_theme.c_str())) {
              for (const auto& theme : available_themes) {
                const auto is_selected = (theme == active_theme);
                if (ImGui::Selectable(theme.c_str(), is_selected)) {
                  // utility::logger<"editor">::debug("Changing theme to {}", theme);
                  _editor_theme.apply_theme(theme);
                }
                if (is_selected) {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
            break;
          }
          default: {
            ImGui::Text("Not implemented yet");
            break;
          }
        }
        ImGui::EndChild();
      }

      ImGui::Dummy(ImVec2(0.0f, 10.0f));

      const auto button_width = 75.0f;
      const auto button_spacing = 10.0f;
      const auto total_button_width = (button_width * 2.0f) + button_spacing;

      const auto button_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - total_button_width;
      ImGui::SetCursorPosX(button_x);

      if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if (ImGui::Button("Apply", ImVec2(button_width, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (_open_popups.has(popup::menu_about)) {
      _open_popups.clear(popup::menu_about);
      ImGui::OpenPopup("About");
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2{600, 200}, ImVec2{FLT_MAX, FLT_MAX});

    if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Sandbox Game Engine");
      ImGui::Text("Copyright (C) 2025 KAJDEV");

      ImGui::Dummy(ImVec2(0.0f, 5.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 5.0f));

      ImGuiStyle& style = ImGui::GetStyle();
      const auto saved_padding_x = style.WindowPadding.x;

      // Match outer padding (optional tweak)
      style.WindowPadding.x = 0.0f;

      const auto child_size = ImVec2(ImGui::GetContentRegionAvail().x, 200.0f);

      if (ImGui::BeginChild("AboutTextRegion", child_size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::TextWrapped("This game engine is a custom-built Vulkan-based engine designed from the ground up in modern C++20.");
        ImGui::TextWrapped("It features a fully custom math library, a component-based ECS architecture inspired by EnTT, and a scene");
        ImGui::TextWrapped("hierarchy system for organizing game objects in a flexible and scalable way.");

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::TextWrapped("Rendering is handled using a forward renderer with support for GPU-driven rendering techniques, including");
        ImGui::TextWrapped("indirect draw calls and compute shader-based culling. Assets like meshes and textures are loaded via a custom");
        ImGui::TextWrapped("asset pipeline, with support for glTF model imports and custom binary formats.");

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::TextWrapped("The engine is modular and data-driven, with a descriptor system for resource binding, a render graph for");
        ImGui::TextWrapped("pass scheduling, and support for advanced features like skeletal animation, PBR materials, and dynamic terrain.");
        ImGui::TextWrapped("It is designed to serve as a flexible foundation for both sandbox experimentation and full game development.");
      
        ImGui::EndChild();
      }

      style.WindowPadding.x = saved_padding_x;

      ImGui::Dummy(ImVec2(0.0f, 5.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 5.0f));

      ImGui::Text("Version: v" SBX_CORE_VERSION_STRING "+" SBX_COMPILE_TIMESTAMP);
      ImGui::Text("Branch: " SBX_BRANCH);
      ImGui::Text("Commit: " SBX_COMMIT_HASH);

      ImGui::Dummy(ImVec2(0.0f, 5.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 5.0f));

      ImGui::Text("License: MIT License");

      ImGui::Dummy(ImVec2(0.0f, 5.0f));

      // Right-align buttons
      const auto button_width = 75.0f;

      // Align cursor X to the right
      const auto button_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button_width;
      ImGui::SetCursorPosX(button_x);

      if (ImGui::Button("Close", ImVec2(button_width, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    ImGui::PopStyleColor();
  }

  _hierarchy_panel.render();
  _profiler_panel.render();
  _nodes_panel.render();
  _log_panel.render();
  _property_panel.render(_hierarchy_panel.selected_node_id());

  {
    ImGui::Begin("Target");

    if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
      static const auto targets = std::array<std::string_view, 12u>{
        "shadow",
        "albedo",
        "normal",
        "position",
        "material",
        "emissive",
        "object_id",
        "linear_depth",
        "resolve",
        "brightness",
        "tonemap",
        "fxaa"
      };

      static auto current_target = targets.size() - 1u;

      if (ImGui::BeginCombo("Render Target", targets[current_target].data())) {
        for (auto i = 0u; i < targets.size(); ++i) {
          const auto is_selected = (current_target == i);

          if (ImGui::Selectable(targets[i].data(), is_selected)) {
            current_target = i;
            _attachment_name = targets[i];
          }

          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }

        ImGui::EndCombo();
      }
    }

    ImGui::End();
  }

  {
    ImGui::Begin("Scene");

    auto available_size = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::Image(reinterpret_cast<ImTextureID>(_descriptor_handler.descriptor_set()), available_size);
    ImGui::PopStyleVar();

    if (ImGui::IsItemHovered()) {
      ImGuiIO& io = ImGui::GetIO();
      io.WantCaptureMouse = false; 
      io.WantCaptureKeyboard = false;
    }

    auto scene_min = ImVec2{};
    auto scene_max = ImVec2{};


    scene_min = ImGui::GetWindowContentRegionMin();
    scene_max = ImGui::GetWindowContentRegionMax();

    scene_min.x += ImGui::GetWindowPos().x;
    scene_min.y += ImGui::GetWindowPos().y;
    scene_max.x += ImGui::GetWindowPos().x;
    scene_max.y += ImGui::GetWindowPos().y;

    auto width = scene_max.x - scene_min.x;
    auto height = scene_max.y - scene_min.y;

    _viewport_size = sbx::math::vector2u{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};

    auto mouse_scene = ImGui::GetMousePos();

    _mouse_position = { mouse_scene.x - scene_min.x, mouse_scene.y - scene_min.y};

    _mouse_position = math::vector2{
      std::clamp(_mouse_position.x(), 0.0f, static_cast<std::float_t>(_viewport_size.x())), 
      std::clamp(_mouse_position.y(), 0.0f, static_cast<std::float_t>(_viewport_size.y()))
    };

    ImGui::End();
  }
}

auto editor_subrenderer::_save() -> void {
  // [TODO] KAJ 2024-12-02 : Implement save
}

auto editor_subrenderer::_load() -> void {
  // [TODO] KAJ 2024-12-02 : Implement load
}

auto editor_subrenderer::_undo() -> void {
  // [TODO] KAJ 2024-12-02 : Implement undo
}

} // namespace sbx::editor