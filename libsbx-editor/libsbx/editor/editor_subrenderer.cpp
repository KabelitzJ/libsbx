// SPDX-License-Identifier: MIT
#include <libsbx/editor/editor_subrenderer.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

namespace sbx::editor {

auto _format_bytes(std::size_t bytes) -> std::string {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};

  auto unit_index = std::int32_t{0};

  auto size = static_cast<std::double_t>(bytes);

  while (size >= 1024u && unit_index < 4u) {
    size /= 1024u;
    unit_index++;
  }

  return fmt::format("{:.2f} {} ({})", size, units[unit_index], bytes);
}

auto _get_dense_memory_flags(VkMemoryPropertyFlags flags) -> std::string {
  auto result = std::string{};

  result += (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "D" : "-";
  result += (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? "V" : "-";
  result += (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "C" : "-";
  result += (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "H" : "-";
  result += (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) ? "L" : "-";
  result += (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) ? "P" : "-";
  
  return result;
}

struct byte_unit {
  std::double_t divisor_bytes;
  const char* name;
}; // struct byte_unit

static auto choose_byte_unit(std::double_t peak_bytes) -> byte_unit {
  static constexpr auto kib = 1024.0;
  static constexpr auto mib = kib * kib;
  static constexpr auto gib = mib * kib;
  static constexpr auto tib = gib * kib;

  if (peak_bytes >= tib) {
    return byte_unit{tib, "TiB"};
  }

  if (peak_bytes >= gib) {
    return byte_unit{gib, "GiB"};
  }

  if (peak_bytes >= mib) {
    return byte_unit{mib, "MiB"};
  }

  if (peak_bytes >= kib) {
    return byte_unit{kib, "KiB"};
  }

  return byte_unit{1.0, "B"};
}

static auto ring_start(std::uint32_t head, std::uint32_t count, std::uint32_t cap) -> std::uint32_t {
  return (head + cap - count) % cap;
}

static auto ring_peak_in_window(const std::double_t* xs, const std::double_t* ys, std::uint32_t cap, std::uint32_t head, std::uint32_t count, std::double_t x_min) -> std::double_t {
  if (count == 0u) {
    return 0.0;
  }

  auto start = ring_start(head, count, cap);

  auto peak = 0.0;
  for (auto i = 0u; i < count; ++i) {
    auto idx = (start + i) % cap;

    if (xs[idx] >= x_min) {
      peak = std::max(peak, std::abs(ys[idx]));
    }
  }

  return peak;
}

struct ring_view {
  const std::double_t* xs = nullptr;
  const std::double_t* ys = nullptr;
  std::uint32_t cap = 0u;
  std::uint32_t start = 0u;
  std::double_t y_mul = 1.0;
}; // struct ring_view

static auto ring_getter(int idx, void* data) -> ImPlotPoint {
  const auto* v = static_cast<const ring_view*>(data);

  auto i = (v->start + static_cast<std::uint32_t>(idx)) % v->cap;

  return ImPlotPoint{v->xs[i], v->ys[i] * v->y_mul};
}

static auto plot_ring_scaled(const char* label, const std::double_t* xs, const std::double_t* ys, std::uint32_t cap, std::uint32_t head, std::uint32_t count, std::double_t y_mul) -> void {
  if (count == 0u) {
    return;
  }

  auto view = ring_view{};
  view.xs = xs;
  view.ys = ys;
  view.cap = cap;
  view.start = ring_start(head, count, cap);
  view.y_mul = y_mul;

  ImPlot::PlotLineG(label, ring_getter, &view, static_cast<int>(count));
}

editor_subrenderer::editor_subrenderer(const memory::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name)
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
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& allocator = graphics_module.allocator();

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
      static const auto targets = std::array<std::string_view,15u>{
        "shadow0",
        "shadow1",
        "shadow2",
        "shadow3",
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

  {
    if (ImGui::Begin("Memory Statistics")) {
      if (ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto statistics = VmaTotalStatistics{};
        vmaCalculateStatistics(allocator, &statistics);

        const VkPhysicalDeviceMemoryProperties* memory_properties = nullptr;
        vmaGetMemoryProperties(allocator, &memory_properties);

        if (ImGui::BeginTable("##global_summary_table", 2, ImGuiTableFlags_None)) {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto render_summary_row = [&](const char* label, const std::string& value) -> void {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value.c_str());
          };

          render_summary_row("Total Allocated", _format_bytes(statistics.total.statistics.allocationBytes));
          render_summary_row("Total Block Bytes", _format_bytes(statistics.total.statistics.blockBytes));
          render_summary_row("Allocation Count", std::to_string(statistics.total.statistics.allocationCount));
          render_summary_row("Block Count", std::to_string(statistics.total.statistics.blockCount));

          ImGui::EndTable();
        }

        ImGui::Spacing();

        constexpr auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg;

        if (ImGui::BeginTable("VmaAllocTable", 5, table_flags)) {
          ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 30.0f);
          ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f);
          ImGui::TableSetupColumn("Allocations");
          ImGui::TableSetupColumn("Size");
          ImGui::TableSetupColumn("Frag %");
          ImGui::TableHeadersRow();

          for (auto i = 0u; i < memory_properties->memoryTypeCount; ++i) {
            auto& type_stats = statistics.memoryType[i].statistics;

            if (type_stats.blockCount == 0) {
              continue;
            }

            auto property_flags = memory_properties->memoryTypes[i].propertyFlags;

            ImGui::TableNextRow();

            // 1. Index
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);

            // 2. Dense Symbols
            ImGui::TableSetColumnIndex(1);
            
            auto render_symbol = [](bool condition, const char* symbol) -> void {
              ImGui::Text("%s", condition ? symbol : "-");
              ImGui::SameLine(0.0f, 0.0f);
            };

            ImGui::BeginGroup();
            render_symbol((property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), "D"); 
            render_symbol((property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), "V"); 
            render_symbol((property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), "C"); 
            render_symbol((property_flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT), "H"); 
            render_symbol((property_flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT), "L"); 
            render_symbol((property_flags & VK_MEMORY_PROPERTY_PROTECTED_BIT), "P"); 
            ImGui::EndGroup();

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("D: Device Local\nV: Host Visible\nC: Host Coherent\nH: Host Cached\nL: Lazily Allocated\nP: Protected");
            }

            // 3. Allocations
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", type_stats.allocationCount);

            // 4. Size
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(_format_bytes(type_stats.allocationBytes).c_str());

            // 5. Fragmentation
            ImGui::TableSetColumnIndex(4);
        
            const auto total_free = type_stats.blockBytes - type_stats.allocationBytes;
            
            if (total_free > 0 && type_stats.allocationCount > 1) {
              const auto frag_score = 1.0f - (static_cast<std::float_t>(type_stats.allocationBytes) / static_cast<std::float_t>(type_stats.blockBytes));
              const auto color = (frag_score <= 0.5f) ? ImVec4{1.0f, 1.0f, 1.0f, 1.0f} : ImVec4{1.0f, 0.4f, 0.4f, 1.0f};

              ImGui::TextColored(color, "%.1f%%", frag_score * 100.0f);
            } else {
              ImGui::TextDisabled("0.0%%");
            }
          }

          ImGui::EndTable();
        }
      }

      if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& tracker = memory::memory_tracker::instance();
        auto total = tracker.total_statistics();

        static constexpr auto category_count = magic_enum::enum_count<memory::allocation_category>();

        static auto selected_valid = false;
        static auto selected_category = magic_enum::enum_value<memory::allocation_category>(0);

        ImGui::Text("Live allocations: %zu", total.current_allocations());
        ImGui::Text("Live bytes: %s", _format_bytes(total.current_bytes()).c_str());
        ImGui::Text("Peak bytes (max category): %s", _format_bytes(total.peak_bytes).c_str());

        ImGui::Separator();

        static constexpr auto plot_capacity = 600;

        struct ui_state {
          std::array<memory::allocation_statistics_snapshot, category_count> prev{};
          std::array<std::float_t, category_count> ema_alloc_bytes_per_s{};
          std::array<std::float_t, category_count> ema_allocs_per_s{};
          bool initialized{false};

          memory::allocation_statistics_snapshot prev_total{};
          std::float_t ema_total_alloc_bytes_per_s{0.0f};
          bool total_initialized{false};

          std::array<std::double_t, plot_capacity> t_s{};
          std::array<std::double_t, plot_capacity> total_live_mib{};
          std::array<std::double_t, plot_capacity> total_alloc_mib_s{};
          std::array<std::double_t, plot_capacity> selected_live_mib{};
          std::array<std::double_t, plot_capacity> selected_alloc_mib_s{};

          std::size_t plot_head{0};
          std::size_t plot_count{0};
          std::double_t time_s{0.0};
        }; // struct ui_state

        static auto state = ui_state{};

        struct row {
          memory::allocation_category category{};
          memory::allocation_statistics_snapshot snap{};

          std::size_t live_bytes{0};
          std::size_t live_allocs{0};
          std::size_t peak_bytes{0};
          std::size_t avg_alloc_size{0};

          std::ptrdiff_t delta_live_bytes{0};
          std::size_t delta_alloc_bytes{0};
          std::size_t delta_free_bytes{0};

          std::float_t alloc_bytes_per_s{0.0f};
          std::float_t allocs_per_s{0.0f};
        }; // struct row

        auto dt = static_cast<std::float_t>(ImGui::GetIO().DeltaTime);

        if (dt < 0.000001f) {
          dt = 0.000001f;
        }

        auto alpha = 0.0f;

        {
          auto tau = 0.25f;
          alpha = 1.0f - std::exp(-dt / tau);
        }

        auto to_mib = [&](std::size_t bytes) -> std::double_t {
          return static_cast<std::double_t>(bytes) / (1024.0 * 1024.0);
        };

        auto to_mib_s = [&](std::float_t bytes_per_s) -> std::double_t {
          return static_cast<std::double_t>(bytes_per_s) / (1024.0 * 1024.0);
        };

        auto format_signed_bytes = [&](std::ptrdiff_t v) -> std::string {
          if (v == 0) {
            return "0";
          }

          if (v > 0) {
            return "+" + _format_bytes(static_cast<std::size_t>(v));
          }

          return "-" + _format_bytes(static_cast<std::size_t>(-v));
        };

        auto column_count = 5;

        auto selected_live_mib = 0.0;
        auto selected_alloc_mib_s = 0.0;
        auto have_selected_series = false;

        auto rows = std::vector<row>{};
        rows.reserve(category_count);

        for (const auto category : magic_enum::enum_values<memory::allocation_category>()) {
          auto snap = tracker.statistics(category);

          auto r = row{};
          r.category = category;
          r.snap = snap;

          r.live_bytes = snap.current_bytes();
          r.live_allocs = snap.current_allocations();
          r.peak_bytes = snap.peak_bytes;

          if (r.live_allocs != 0u) {
            r.avg_alloc_size = r.live_bytes / r.live_allocs;
          }

          auto index_opt = magic_enum::enum_index(category);
          if (!index_opt) {
            rows.push_back(r);
            continue;
          }

          auto index = static_cast<std::size_t>(*index_opt);

          if (state.initialized) {
            const auto& prev = state.prev[index];

            r.delta_alloc_bytes = snap.bytes_allocated - prev.bytes_allocated;
            r.delta_free_bytes = snap.bytes_freed - prev.bytes_freed;

            r.delta_live_bytes = static_cast<std::ptrdiff_t>(snap.current_bytes()) - static_cast<std::ptrdiff_t>(prev.current_bytes());

            auto alloc_bytes_per_s = static_cast<std::float_t>(r.delta_alloc_bytes) / dt;
            auto allocs_per_s = static_cast<std::float_t>(snap.allocation_count - prev.allocation_count) / dt;

            state.ema_alloc_bytes_per_s[index] = state.ema_alloc_bytes_per_s[index] + alpha * (alloc_bytes_per_s - state.ema_alloc_bytes_per_s[index]);

            state.ema_allocs_per_s[index] = state.ema_allocs_per_s[index] + alpha * (allocs_per_s - state.ema_allocs_per_s[index]);

            r.alloc_bytes_per_s = state.ema_alloc_bytes_per_s[index];
            r.allocs_per_s = state.ema_allocs_per_s[index];
          }

          state.prev[index] = snap;

          if (selected_valid) {
            if (category == selected_category) {
              selected_live_mib = to_mib(r.live_bytes);
              selected_alloc_mib_s = to_mib_s(r.alloc_bytes_per_s);
              have_selected_series = true;
            }
          }

          rows.push_back(r);
        }

        if (ImGui::BeginTable("cpu_memory_stats", column_count, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
          ImGui::TableSetupColumn("Category");
          ImGui::TableSetupColumn("Live Bytes", ImGuiTableColumnFlags_DefaultSort);
          ImGui::TableSetupColumn("Live Allocs");
          ImGui::TableSetupColumn("Peak Bytes");
          ImGui::TableSetupColumn("Avg Size");

          ImGui::TableHeadersRow();

          if (auto* sort_specs = ImGui::TableGetSortSpecs(); sort_specs && sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
            const auto spec = sort_specs->Specs[0];
            const auto asc = spec.SortDirection == ImGuiSortDirection_Ascending;
            const auto col = spec.ColumnIndex;

            auto less = [&](const row& a, const row& b) -> bool {
              auto cmp = [&](auto lhs, auto rhs) -> bool {
                if (asc) {
                  return lhs < rhs;
                }

                return lhs > rhs;
              };

              if (col == 0) {
                return cmp(static_cast<int>(a.category), static_cast<int>(b.category));
              }

              if (col == 1) {
                return cmp(a.live_bytes, b.live_bytes);
              }

              if (col == 2) {
                return cmp(a.live_allocs, b.live_allocs);
              }

              if (col == 3) {
                return cmp(a.peak_bytes, b.peak_bytes);
              }

              if (col == 4) {
                return cmp(a.avg_alloc_size, b.avg_alloc_size);
              }

              return cmp(a.live_bytes, b.live_bytes);
            };

            std::sort(rows.begin(), rows.end(), less);
            sort_specs->SpecsDirty = false;
          }

          for (const auto& r : rows) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(r.category));

            auto is_selected = selected_valid && (r.category == selected_category);
            if (ImGui::Selectable(to_string(r.category).data(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
              selected_valid = true;
              selected_category = r.category;
            }

            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(_format_bytes(r.live_bytes).c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu", r.live_allocs);

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(_format_bytes(r.peak_bytes).c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(_format_bytes(r.avg_alloc_size).c_str());

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
              ImGui::BeginTooltip();

              ImGui::TextUnformatted("This frame:");
              ImGui::Separator();
              ImGui::Text("Delta Live: %s", format_signed_bytes(r.delta_live_bytes).c_str());
              ImGui::Text("Alloc: %s", _format_bytes(r.delta_alloc_bytes).c_str());
              ImGui::Text("Free:  %s", _format_bytes(r.delta_free_bytes).c_str());
              ImGui::Text("Alloc rate (EMA): %s/s", _format_bytes(static_cast<std::size_t>(r.alloc_bytes_per_s)).c_str());

              ImGui::Separator();

              ImGui::TextUnformatted("Totals (since start):");
              ImGui::Separator();
              ImGui::Text("Allocs: %zu", r.snap.allocation_count);
              ImGui::Text("Frees:  %zu", r.snap.deallocation_count);
              ImGui::Text("Alloc bytes: %s", _format_bytes(r.snap.bytes_allocated).c_str());
              ImGui::Text("Freed bytes:  %s", _format_bytes(r.snap.bytes_freed).c_str());

              ImGui::EndTooltip();
            }
          }

          ImGui::EndTable();
        }

        ImGui::Separator();

        if (selected_valid) {
          auto name = memory::to_string(selected_category);

          ImGui::Text("Selected category: %s", name.data());
        } else {
          ImGui::Text("Selected category: None");
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear selection")) {
          selected_valid = false;
        }

        ImGui::Separator();

        state.initialized = true;

        auto total_alloc_bytes_per_s = 0.0f;

        if (state.total_initialized) {
          auto delta_total_alloc = total.bytes_allocated - state.prev_total.bytes_allocated;
          total_alloc_bytes_per_s = static_cast<std::float_t>(delta_total_alloc) / dt;

          state.ema_total_alloc_bytes_per_s = state.ema_total_alloc_bytes_per_s + alpha * (total_alloc_bytes_per_s - state.ema_total_alloc_bytes_per_s);
        } else {
          state.total_initialized = true;
          state.ema_total_alloc_bytes_per_s = 0.0f;
        }

        state.prev_total = total;

        state.time_s += static_cast<std::double_t>(dt);

        auto head = state.plot_head;

        state.t_s[head] = state.time_s;
        state.total_live_mib[head] = to_mib(total.current_bytes());
        state.total_alloc_mib_s[head] = to_mib_s(state.ema_total_alloc_bytes_per_s);

        if (have_selected_series) {
          state.selected_live_mib[head] = selected_live_mib;
          state.selected_alloc_mib_s[head] = selected_alloc_mib_s;
        } else {
          state.selected_live_mib[head] = 0.0;
          state.selected_alloc_mib_s[head] = 0.0;
        }

        state.plot_head = (state.plot_head + 1u) % plot_capacity;

        if (state.plot_count < plot_capacity) {
          state.plot_count += 1u;
        }

        static constexpr auto kib = 1024.0;
        static constexpr auto mib = kib * kib;

        auto window_s = 30.0;
        auto now_s = state.time_s;
        auto min_s = now_s - window_s;

        if (min_s < 0.0) {
          min_s = 0.0;
        }

        auto peak_live_mib = ring_peak_in_window(state.t_s.data(), state.total_live_mib.data(), plot_capacity, state.plot_head, state.plot_count, min_s);

        if (have_selected_series) {
          auto peak_sel = ring_peak_in_window(state.t_s.data(), state.selected_live_mib.data(), plot_capacity, state.plot_head, state.plot_count, min_s);

          peak_live_mib = std::max(peak_live_mib, peak_sel);
        }

        auto peak_live_bytes = peak_live_mib * mib;
        auto live_unit = choose_byte_unit(peak_live_bytes);
        auto live_y_mul = mib / live_unit.divisor_bytes;

        auto live_ylabel = std::array<char, 32>{};
        std::snprintf(live_ylabel.data(), live_ylabel.size(), "%s", live_unit.name);

        if (ImPlot::BeginPlot("Live Bytes##cpu_mem_live", ImVec2(-1.0f, 140.0f))) {
          ImPlot::SetupAxes(nullptr, live_ylabel.data(), ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_AutoFit);
          ImPlot::SetupAxisLimits(ImAxis_X1, min_s, now_s, ImGuiCond_Always);

          plot_ring_scaled("Total", state.t_s.data(), state.total_live_mib.data(), plot_capacity, state.plot_head, state.plot_count, live_y_mul);

          if (have_selected_series) {
            plot_ring_scaled("Selected", state.t_s.data(), state.selected_live_mib.data(), plot_capacity, state.plot_head, state.plot_count, live_y_mul);
          }

          ImPlot::EndPlot();
        }

        auto peak_rate_mib_s = ring_peak_in_window(state.t_s.data(), state.total_alloc_mib_s.data(), plot_capacity, state.plot_head, state.plot_count, min_s);

        if (have_selected_series) {
          auto peak_sel = ring_peak_in_window(state.t_s.data(), state.selected_alloc_mib_s.data(), plot_capacity, state.plot_head, state.plot_count, min_s);

          peak_rate_mib_s = std::max(peak_rate_mib_s, peak_sel);
        }

        auto peak_rate_bytes_s = peak_rate_mib_s * mib;
        auto rate_unit = choose_byte_unit(peak_rate_bytes_s);
        auto rate_y_mul = mib / rate_unit.divisor_bytes;

        auto rate_ylabel = std::array<char, 32>{};
        std::snprintf(rate_ylabel.data(), rate_ylabel.size(), "%s/s", rate_unit.name);

        if (ImPlot::BeginPlot("Allocation Rate##cpu_mem_rate", ImVec2(-1.0f, 120.0f))) {
          ImPlot::SetupAxes(nullptr, rate_ylabel.data(), ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_AutoFit);
          ImPlot::SetupAxisLimits(ImAxis_X1, min_s, now_s, ImGuiCond_Always);

          plot_ring_scaled("Total", state.t_s.data(), state.total_alloc_mib_s.data(), plot_capacity, state.plot_head, state.plot_count, rate_y_mul);

          if (have_selected_series) {
            plot_ring_scaled("Selected", state.t_s.data(), state.selected_alloc_mib_s.data(), plot_capacity, state.plot_head, state.plot_count, rate_y_mul);
          }

          ImPlot::EndPlot();
        }
      }

    }

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