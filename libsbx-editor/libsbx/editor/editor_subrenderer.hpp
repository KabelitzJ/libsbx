#ifndef LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_
#define LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_

#include <deque>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
// #include <ImGuizmo.h>

#include <libsbx/editor/bindings/imgui.hpp>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/version.hpp>
#include <libsbx/core/profiler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/devices/devices_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/tag.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/models/static_mesh_subrenderer.hpp>

#include <libsbx/editor/pipeline.hpp>
#include <libsbx/editor/themes.hpp>
#include <libsbx/editor/fonts.hpp>
#include <libsbx/editor/dialog.hpp>
#include <libsbx/editor/menu_bar.hpp>
#include <libsbx/editor/profiler.hpp>

namespace sbx::editor {

class editor_subrenderer final : public sbx::graphics::subrenderer {

  inline static constexpr auto ini_file = std::string_view{"res://data/imgui.ini"};

  using base = sbx::graphics::subrenderer;

  std::string _actual_ini_file;

public:

  editor_subrenderer(const sbx::graphics::render_graph::graphics_pass& pass, const std::filesystem::path& path, const std::string& attachment_name)
  : base{pass},
    _attachment_name{attachment_name},
    _pipeline{path, pass},
    _descriptor_handler{_pipeline, 0u},
    _show_demo_window{false},
    _clear_color{sbx::math::color::black()},
    _selected_node_id{sbx::math::uuid::null()},
    _editor_theme{},
    _has_auto_scroll{true} {
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

    _editor_theme.add_theme("Custom", [this]() { _setup_style(); });

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

  ~editor_subrenderer() override {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
  }

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
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

    // ImGuizmo::BeginFrame();

    // ImGuizmo::SetDrawlist();
    // ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

    _setup_dockspace();
    _setup_windows();

    ImGui::Render();
    auto* draw_data = ImGui::GetDrawData();

    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
  }

private:

  enum class popup : std::uint16_t {
    hierarchy_add_new_node = utility::bit_v<0u>,
    hierarchy_add_component = utility::bit_v<1u>,
    hierarchy_delete = utility::bit_v<2u>,
    menu_preferences = utility::bit_v<3u>,
    menu_about = utility::bit_v<4u>,
  }; // enum class popup

  auto _setup_dockspace() -> void {
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

  auto _context_menu(const sbx::scenes::node node) -> void {
    auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scene_module.scene();

    if (ImGui::BeginPopupContextItem("Actions")) {
      if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("New Node")) {
          _open_popups.set(popup::hierarchy_add_new_node);
        }

        if (ImGui::MenuItem("Component")) {
          _open_popups.set(popup::hierarchy_add_component);
        }

        if (ImGui::MenuItem("Test")) {
          
        }

        ImGui::EndMenu();
      }

      if (_selected_node_id != scene.get_component<sbx::scenes::id>(scene.root())) {
        if (ImGui::MenuItem("Delete")) {
          _open_popups.set(popup::hierarchy_delete);
        }
      }

      ImGui::EndPopup();
    }
  }

  auto _build_tree(const sbx::scenes::node node) -> void {
    auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scene_module.scene();

    if (!scene.is_valid(node)) {
      return;
    }

    const auto& relationship = scene.get_component<sbx::scenes::relationship>(node);

    // auto flag = ImGuiTreeNodeFlags{ImGuiTreeNodeFlags_OpenOnArrow};
    auto flag = ImGuiTreeNodeFlags{ImGuiTreeNodeFlags_OpenOnArrow};

    if (relationship.parent() == sbx::scenes::node::null) {
      flag |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    if (relationship.children().empty()) {
      flag |= ImGuiTreeNodeFlags_Leaf;
    }

    if (scene.get_component<sbx::scenes::id>(node) == _selected_node_id) {
      flag |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(&node);

    if (ImGui::TreeNodeEx(scene.get_component<sbx::scenes::tag>(node).c_str(), flag)) {
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (_selected_node_id != scene.get_component<sbx::scenes::id>(scene.camera()) && _selected_node_id != sbx::math::uuid::null()) {
          if (auto selected_node = scene.find_node(_selected_node_id); selected_node != scenes::node::null) {
            scene.get_component<scenes::selection_tag>(selected_node) = scenes::selection_tag::null;
          }
        }

        _selected_node_id = scene.get_component<sbx::scenes::id>(node);

        utility::logger<"editor">::debug("Selected node id {}", _selected_node_id);

        if (_selected_node_id != scene.get_component<sbx::scenes::id>(scene.camera())) {
          scene.get_component<scenes::selection_tag>(node) = scenes::selection_tag{};
        }
      }

      _context_menu(node);

      for (const auto& child : relationship.children()) {
        if (child != scenes::node::null) {
          _build_tree(child);
        }
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  }

  auto _build_node_preview() -> void {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
    auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scene_module.scene();

    static auto buffer = std::array<char, 32u>{};

    if (auto node = scene.find_node(_selected_node_id); node != scenes::node::null) {
      if (ImGui::TreeNodeEx("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
        buffer.fill('\0');

        auto& tag = scene.get_component<sbx::scenes::tag>(node);

        std::memcpy(buffer.data(), tag.data(), std::min(tag.size(), buffer.size()));

        if (ImGui::InputText("##label", buffer.data(), buffer.size())) {
          tag = buffer.data();
        }

        ImGui::TreePop();
      }

      if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& transform = scene.get_component<sbx::scenes::transform>(node);

        ImGui::Text("Position");

        auto& position = transform.position();

        ImGui::Text("x");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100); // Adjust slider width
        ImGui::DragFloat("##XPosition", &position.x(), 0.1f);
        ImGui::SameLine();

        ImGui::Text("y");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##YPosition", &position.y(), 0.1f);
        ImGui::SameLine();

        ImGui::Text("z");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##ZPosition", &position.z(), 0.1f);

        ImGui::Text("Scale");

        auto& scale = transform.scale();

        ImGui::Text("x");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##XScale", &scale.x(), 0.1f, 0.1f, 10.0f);
        ImGui::SameLine();

        ImGui::Text("y");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##YScale", &scale.y(), 0.1f, 0.1f, 10.0f);
        ImGui::SameLine();

        ImGui::Text("z");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##ZScale", &scale.z(), 0.1f, 0.1f, 10.0f);
        ImGui::SameLine();

        ImGui::TreePop();
      }

      // if (scene.has_component<sbx::models::static_mesh>(node) && ImGui::TreeNodeEx("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
      //   auto& static_mesh = scene.get_component<sbx::models::static_mesh>(node);

      //   for (const auto& submesh : static_mesh.submeshes()) {
      //     auto& material = assets_module.get_asset<models::material>(submesh.material);

      //     ImGui::Text("Features");

      //     auto changed = false;

      //     for (const auto& [type, name] : utility::enum_mapping<models::material_feature>::values) {
      //       auto value = material.features.has(type);

      //       if (ImGui::Checkbox(name.data(), &value)) { 
      //         if (value) {
      //           material.features.set(type); 
      //         } else {
      //           material.features.clear(type);
      //         }

      //         changed = true; 
      //       }
      //     }
      //   }

      //   ImGui::TreePop();
      // }
    }
  }

  auto _setup_windows() -> void {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

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

        // === Row: Sidebar + Content ===
        const auto sidebar_width = 150.0f;
        const auto spacing = ImGui::GetStyle().ItemSpacing.x;

        const auto content_start = ImGui::GetCursorScreenPos();
        const auto total_content_width = ImGui::GetContentRegionAvail().x;

        // Begin Sidebar
        if (ImGui::BeginChild("CategorySidebar", ImVec2(sidebar_width, 0), ImGuiChildFlags_Border)) {
          for (auto i = 0u; i < categories.size(); ++i) {
            if (ImGui::Selectable(categories[i].data(), selected_category == i)) {
              selected_category = i;
            }
          }
          ImGui::EndChild();
        }

        // Sidebar and content on same line
        ImGui::SameLine();

        // Begin Settings Content
        const auto content_width = total_content_width - sidebar_width - spacing;

        if (ImGui::BeginChild("SettingsContent", ImVec2(0, 0), ImGuiChildFlags_None)) {
          switch (selected_category) {
            case 0: { // General
              const auto available_fonts = _editor_font.get_fonts();
              const auto& active_font = _editor_font.get_active_font();

              ImGui::Text("Font");
              if (ImGui::BeginCombo("##FontSelector", active_font.c_str())) {
                for (const auto& font : available_fonts) {
                  const auto is_selected = (font == active_font);

                  if (ImGui::Selectable(font.c_str(), is_selected)) {
                    // utility::logger<"editor">::debug("Changing font to {}", font);
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

        // === Now the buttons ===
        // Add vertical spacing between children and footer buttons
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Right-align buttons
        const auto button_width = 75.0f;
        const auto button_spacing = 10.0f;
        const auto total_button_width = (button_width * 2.0f) + button_spacing;

        // Align cursor X to the right
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

    {
      ImGui::Begin("Hierarchy");

      auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

      auto& scene = scene_module.scene();

      auto root = scene.root();

      auto& relationship = scene.get_component<scenes::relationship>(root);

      for (const auto& child : relationship.children()) {
        if (child != scenes::node::null) {
          _build_tree(child);
        }
      }

      // _build_tree(root);

      const auto custom_backdrop_color = ImVec4{0.2f, 0.2f, 0.2f, 0.5f};

      ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, custom_backdrop_color);

      if (_open_popups.has(popup::hierarchy_add_new_node)) {
        _open_popups.clear(popup::hierarchy_add_new_node);
        ImGui::OpenPopup("New Node");
      }

      // dialog(
      //   "New Node",
      //   [&]() -> bool {
      //     ImGui::Text("Name");
      //     ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      //     ImGui::InputText("##NodeNameInput", _new_name_buffer.data(), _new_name_buffer.size());

      //     const auto name = std::string{_new_name_buffer.data(), std::strlen(_new_name_buffer.data())};

      //     return !name.empty();
      //   },
      //   [&]() {
      //     if (auto node = scene.find_node(_selected_node_id); node != scenes::node::null) {
      //       scene.create_child_node(node, std::string{_new_name_buffer.data()});
      //     } else {
      //       utility::logger<"editor">::warn("No selected node");
      //     }

      //     _new_name_buffer.fill('\0');
      //   },
      //   []() { /* No action on cancel */ }
      // );

      ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

      if (ImGui::BeginPopupModal("New Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Name");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##NodeNameInput", _new_name_buffer.data(), _new_name_buffer.size());

        const auto name = std::string{_new_name_buffer.data(), std::strlen(_new_name_buffer.data())};

        const auto button_width = 75.0f;
        const auto padding = 10.0f;
        const auto available_width = ImGui::GetContentRegionAvail().x;
        const auto total_width = (button_width * 2.0f) + padding;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - total_width);

        const auto footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - footer_height);

        if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
          _new_name_buffer.fill('\0');
          ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (name.empty()) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create", ImVec2{button_width, 0})) {
          if (auto node = scene.find_node(_selected_node_id); node != scenes::node::null) {
            scene.create_child_node(node, name);
          } else {
            utility::logger<"editor">::warn("No selected node");
          }

          _new_name_buffer.fill('\0');
          ImGui::CloseCurrentPopup();
        }

        if (name.empty()) {
          ImGui::EndDisabled();
        }

        ImGui::EndPopup();
      }

      if (_open_popups.has(popup::hierarchy_add_component)) {
        _open_popups.clear(popup::hierarchy_add_component);
        ImGui::OpenPopup("Add Component");
      }

      ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

      if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Not implemented");

        if (ImGui::Button("Close")) {
          ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
      }

      if (_open_popups.has(popup::hierarchy_delete)) {
        _open_popups.clear(popup::hierarchy_delete);
        ImGui::OpenPopup("Delete Node");
      }

      ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

      if (ImGui::BeginPopupModal("Delete Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto button_width = 75.0f;
        const auto padding = 10.0f;
        const auto available_width = ImGui::GetContentRegionAvail().x;
        const auto total_width = (button_width * 2.0f) + padding;

        if (auto node = scene.find_node(_selected_node_id); node != scenes::node::null) {
          ImGui::Text("Do you want to delete '%s'", scene.get_component<sbx::scenes::tag>(node).c_str());

          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - total_width);

          const auto footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
          ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - footer_height);

          if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
            ImGui::CloseCurrentPopup();
          }

          ImGui::SameLine();

          if (ImGui::Button("Delete", ImVec2{button_width, 0})) {
            scene.destroy_node(node);
            _selected_node_id = sbx::math::uuid::null();
            ImGui::CloseCurrentPopup();
          }
        } else {
          ImGui::Text("No node selected (This can be considered a programmer error)");

          if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
            ImGui::CloseCurrentPopup();
          }
        }

        ImGui::EndPopup();
      }

      ImGui::PopStyleColor();

      ImGui::End();
    }

    {
      ImGui::Begin("Variables");

      auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

      auto& scene = scenes_module.scene();

      auto camera_node = scene.camera();

      auto& global_light = scene.light();

      if (ImGui::CollapsingHeader("Global Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Color");

        ImGui::PushItemWidth(200.0f);
        ImGui::ColorPicker4("##color_picker", &global_light.color().r(), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview);

        ImGui::SliderFloat("Alpha", &global_light.color().a(), 0.0f, 10.0f);
        ImGui::PopItemWidth();

        // auto direction = global_light.direction();

        // static auto sun_angle = 45.0f * std::numbers::pi_v<std::float_t> / static_cast<std::float_t>(180);
        static auto sun_angle = math::to_radians(math::degree{70.0f});
        static auto tilt = 0.5f;

        ImGui::Text("Sun Angle");
        ImGui::SliderAngle("##sun_angle", sun_angle.data(), 0.0f, 180.0f);

        ImGui::Text("Tilt");
        ImGui::SliderFloat("##tilt", &tilt, 0.0f, 1.0f);

        // Compute direction from angle
        const auto x = math::cos(sun_angle + math::to_radians(math::degree{180.0f}));
        const auto y = math::sin(sun_angle + math::to_radians(math::degree{180.0f}));
        const auto z = -tilt;

        const auto direction = math::vector3::normalized(math::vector3{x, y, z});

        global_light.set_direction(direction);
      }

      auto& settings = core::engine::settings();

      settings.for_each([](const auto& group_name, auto& group) {
        if (ImGui::CollapsingHeader(group_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& entry : group.entries) {
            if (std::holds_alternative<bool>(entry.value.entry)) {
              ImGui::Checkbox(entry.name.c_str(), &std::get<bool>(entry.value.entry));
            } else if (std::holds_alternative<std::uint32_t>(entry.value.entry)) {
              ImGui::DragScalar(entry.name.c_str(), ImGuiDataType_U32, &std::get<std::uint32_t>(entry.value.entry), 1.0f);
            } else if (std::holds_alternative<std::int32_t>(entry.value.entry)) {
              ImGui::DragScalar(entry.name.c_str(), ImGuiDataType_S32, &std::get<std::int32_t>(entry.value.entry), 1.0f);
            } else if (std::holds_alternative<std::float_t>(entry.value.entry)) {
              const auto* min = std::holds_alternative<std::float_t>(entry.value.min) ? &std::get<std::float_t>(entry.value.min) : nullptr;
              const auto* max = std::holds_alternative<std::float_t>(entry.value.max) ? &std::get<std::float_t>(entry.value.max) : nullptr;

              ImGui::DragScalar(entry.name.c_str(), ImGuiDataType_Float, &std::get<std::float_t>(entry.value.entry), 0.001f, min, max);
            } else if (std::holds_alternative<std::string>(entry.value.entry)) {
              auto& str = std::get<std::string>(entry.value.entry);
              ImGui::InputText(entry.name.c_str(), str.data(), str.size());
            }
          }
        }
      });

      ImGui::End();
    }

    {
      ImGui::Begin("Stats");

      auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

      auto& scene = scene_module.scene();

      const auto delta_time = sbx::core::engine::delta_time();

      _time += delta_time;
      ++_frames;

      if (_time >= sbx::units::second{1}) {
        _fps = _frames;
        _time = sbx::units::second{0};
        _frames = 0;
      }

      if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto ms = units::quantity_cast<sbx::units::millisecond>(delta_time);
        ImGui::Text("Delta time");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(ms));
        ImGui::Text("  %.3f [ms]", ms.value());
        ImGui::PopStyleColor();
        // ImGui::SameLine();
        // ImGui::PushStyleColor(ImGuiCol_PlotHistogram, _get_color_for_time(ms));
        // ImGui::ProgressBar(ms.value() / 16.66, ImVec2(200, 15));
        // ImGui::PopStyleColor();

        ImGui::Text("FPS          %d", _fps);
      }

      if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
        show_profiler();

        // if (ImGui::BeginTable("profiler_table", 2,
        //       ImGuiTableFlags_BordersInnerV |
        //       ImGuiTableFlags_BordersOuter |
        //       ImGuiTableFlags_RowBg |
        //       ImGuiTableFlags_SizingStretchProp)) {

        //   ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        //   ImGui::TableSetupColumn("Time [ms]", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        //   ImGui::TableHeadersRow();

        //   core::engine::profiler().for_each([](const auto& group_name, const auto& group) {
        //     // Insert a bold group header that spans both columns
        //     ImGui::TableNextRow();
        //     ImGui::TableSetColumnIndex(0);
        //     ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(50, 60, 80, 255)); // optional group row color
        //     ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        //     ImGui::TextUnformatted(group_name.c_str());
        //     ImGui::PopStyleColor();
        //     ImGui::TableSetColumnIndex(1);
        //     ImGui::TextDisabled(" "); // Empty to keep row height aligned

        //     // Add total time
        //     if (group.overall.value() > 0.0f) {
        //       const auto ms = units::quantity_cast<sbx::units::millisecond>(group.overall);

        //       ImGui::TableNextRow();
        //       ImGui::TableSetColumnIndex(0);
        //       ImGui::TextUnformatted("total");

        //       ImGui::TableSetColumnIndex(1);
        //       ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(ms));
        //       ImGui::Text("%.3f", ms.value());
        //       ImGui::PopStyleColor();
        //     }

        //     // Add entries
        //     for (const auto& [name, measurement] : group.entries) {
        //       const auto ms = units::quantity_cast<sbx::units::millisecond>(measurement);

        //       ImGui::TableNextRow();
        //       ImGui::TableSetColumnIndex(0);
        //       ImGui::TextUnformatted(name.c_str());

        //       ImGui::TableSetColumnIndex(1);
        //       ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(ms));
        //       ImGui::Text("%.3f", ms.value());
        //       ImGui::PopStyleColor();
        //     }
        //   });

        //   ImGui::EndTable();
        // }
      }

      if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        static auto current_target = std::uint32_t{12};
        const auto targets = std::vector<std::string_view>{
          "albedo",
          "normal",
          "position",
          "material",
          "emissive",
          "object_id",
          "linear_depth",
          "ssao",
          "resolve",
          "brightness",
          "bloom_full",
          "tonemap",
          "fxaa",
          "selection"
        };

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

      // static constexpr auto max_time = sbx::units::second{5};

      // _elapsed += delta_time;

      // _deltas.push_back(sbx::units::quantity_cast<sbx::units::millisecond>(delta_time).value());
      // _time_stamps.push_back(_elapsed.value());

      // while (!_time_stamps.empty() && (_elapsed - sbx::units::second{_time_stamps.front()} > max_time)) {
      //   _deltas.erase(_deltas.begin());
      //   _time_stamps.erase(_time_stamps.begin());
      // }

      // if (ImPlot::BeginPlot("Delta Time Graph", ImVec2{-1, 150})) {
      //   ImPlot::SetupAxes("Time (s)", "Delta Time (ms)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
      //   ImPlot::SetupAxisLimits(ImAxis_X1, (_elapsed - max_time).value(), _elapsed.value(), ImGuiCond_Always);
      //   ImPlot::PlotShaded("Delta Time", _time_stamps.data(), _deltas.data(), _deltas.size(), 0.0f);
      //   ImPlot::EndPlot();
      // }

      ImGui::End();
    }

    {
      ImGui::Begin("Nodes");

      ImNodes::BeginNodeEditor();

      static auto links = std::vector<std::pair<std::int32_t, std::int32_t>>{};

      static auto color = ImVec4{1.0f, 0.0f, 0.0f, 1.0f};

      if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("editor_context_menu");
      }

      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

      if (ImGui::BeginPopup("editor_context_menu")) {
        if (ImGui::MenuItem("Color")) {
          
        }
        if (ImGui::MenuItem("Add")) {

        }
        if (ImGui::MenuItem("Subtract")) {

        }
        if (ImGui::MenuItem("Multiply")) {

        }
        if (ImGui::MenuItem("Remap")) {

        }
        ImGui::EndPopup();
      }

      ImGui::PopStyleVar();

      ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(250, 0, 0, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(255, 0, 0, 255));
      ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(255, 0, 0, 255));
      ImNodes::BeginNode(0);

      ImNodes::BeginNodeTitleBar();
      ImGui::TextUnformatted("Color");
      ImNodes::EndNodeTitleBar();

      ImNodes::BeginOutputAttribute(1);
      ImGui::SetNextItemWidth(150);
      ImGui::DragFloat("##red", &color.x, 0.001f, 0.0f, 1.0f);
      ImGui::SameLine();
      ImGui::Text("  Red");
      ImNodes::EndOutputAttribute();  

      ImNodes::BeginOutputAttribute(2);
      ImGui::SetNextItemWidth(150);
      ImGui::DragFloat("##green", &color.y, 0.001f, 0.0f, 1.0f);
      ImGui::SameLine();
      ImGui::Text("Green");
      ImNodes::EndOutputAttribute();

      ImNodes::BeginOutputAttribute(3);
      ImGui::SetNextItemWidth(150);
      ImGui::DragFloat("##blue", &color.z, 0.001f, 0.0f, 1.0f);
      ImGui::SameLine();
      ImGui::Text(" Blue");
      ImNodes::EndOutputAttribute();

      ImNodes::BeginOutputAttribute(4);
      ImGui::SetNextItemWidth(150);
      ImGui::DragFloat("##alpha", &color.w, 0.001f, 0.0f, 1.0f);
      ImGui::SameLine();
      ImGui::Text("Alpha");
      ImNodes::EndOutputAttribute();

      ImGui::Dummy(ImVec2(0.0f, 10.0f));

      ImGui::SetNextItemWidth(200);
      ImGui::ColorPicker4("##color_picker", &color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview);

      ImNodes::EndNode();
      ImNodes::PopColorStyle();
      ImNodes::PopColorStyle();
      ImNodes::PopColorStyle();

      ImNodes::BeginNode(5);

      ImNodes::BeginNodeTitleBar();
      ImGui::TextUnformatted("add");
      ImNodes::EndNodeTitleBar();

      ImNodes::BeginInputAttribute(6);
      ImGui::Text("in");
      ImNodes::EndInputAttribute();

      ImGui::Dummy(ImVec2(20.0f, 0.0f));

      ImNodes::BeginOutputAttribute(7);
      ImGui::Text("out");
      ImNodes::EndOutputAttribute();

      ImNodes::BeginInputAttribute(8);
      ImGui::Text("in");
      ImNodes::EndInputAttribute();

      ImNodes::EndNode();

      for (auto i = 0; i < links.size(); ++i) {
        const auto link = links[i];
        ImNodes::Link(i, link.first, link.second);
      }

      ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);

      ImNodes::EndNodeEditor();

      auto start = 0;
      auto end = 0;

      if (ImNodes::IsLinkCreated(&start, &end)) {
        links.emplace_back(start, end);
      }

      ImGui::End();
    }

    auto vMax = ImVec2{};
    auto vMin = ImVec2{};

    {
      ImGui::Begin("Scene");

      // ImGuizmo::SetDrawlist();

      // const auto window_position = ImGui::GetWindowPos();
      // const auto window_size = ImGui::GetWindowSize();

      // ImGuizmo::SetRect(window_position.x, window_position.y, window_size.x, window_size.y);

      auto available_size = ImGui::GetContentRegionAvail();

      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
      ImGui::Image(reinterpret_cast<ImTextureID>(_descriptor_handler.descriptor_set()), available_size);
      ImGui::PopStyleVar();

      // auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

      // auto& scene = scenes_module.scene();

      // auto camera_node = scene.camera();

      // auto& global_light = scene.light();

      // auto direction = math::vector3::normalized(global_light.direction());

      // const auto origin = math::vector3::zero;
      // const auto up = math::vector3::up;

      // auto light_matrix = math::matrix4x4::inverted(math::matrix4x4::look_at(origin, origin + direction, up));

      // const auto& camera = scene.get_component<scenes::camera>(camera_node);
      // const auto& camera_transform = scene.get_component<scenes::transform>(camera_node);

      // const auto camera_view = math::matrix4x4::inverted(camera_transform.as_matrix());
      // const auto camera_projection = camera.projection(0.1f, 100.0f);

      // ImGuizmo::Manipulate(camera_view.data(), camera_projection.data(), ImGuizmo::ROTATE, ImGuizmo::WORLD, light_matrix.data());

      // global_light.set_direction(-math::vector3{light_matrix[2]});

      if (ImGui::IsItemHovered()) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = false; 
        io.WantCaptureKeyboard = false;
      }

      vMin = ImGui::GetWindowContentRegionMin();
      vMax = ImGui::GetWindowContentRegionMax();

      vMin.x += ImGui::GetWindowPos().x;
      vMin.y += ImGui::GetWindowPos().y;
      vMax.x += ImGui::GetWindowPos().x;
      vMax.y += ImGui::GetWindowPos().y;

      ImGui::End();
    }

    {
      ImGui::Begin("Properties");

      auto width = vMax.x - vMin.x;
      auto height = vMax.y - vMin.y;

      _viewport_size = sbx::math::vector2u{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};

      ImGui::Text("Width: %f", width);
      ImGui::Text("Height: %f", height);
      ImGui::Text("Aspect Ratio: %f", width / height);

      _build_node_preview();

      ImGui::End();
    }

    {
      ImGui::Begin("Log");

      const auto lines = utility::detail::sink()->lines();
    
      if (ImGui::Button("Clear"))  {
        utility::detail::sink()->clear();
      }

      ImGui::SameLine();

      ImGui::Checkbox("Auto-Scroll", &_has_auto_scroll);

      ImGui::Separator();

      ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

      for (const auto& [msg, level] : lines) {
        ImGui::PushStyleColor(ImGuiCol_Text, _log_color(level));
        ImGui::TextUnformatted(msg.c_str());
        ImGui::PopStyleColor();
      }

      if (_has_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }

      ImGui::EndChild();

      ImGui::End();
    }
  }

  auto _setup_style() -> void {
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    
    auto* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 0.37f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 0.16f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 0.57f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.21f, 0.27f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.73f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.10f, 0.15f, 0.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.19f, 0.41f, 0.78f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.25f, 0.29f, 0.80f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 0.20f);

    // General
    // ImGuiStyle& style = ImGui::GetStyle();
    // ImVec4* colors = style.Colors;

    // colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    // colors[ImGuiCol_TextDisabled] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    // colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
    // colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
    // colors[ImGuiCol_Border] = ImVec4(0.04f, 0.04f, 0.04f, 0.99f);
    // colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    // colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
    // colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
    // colors[ImGuiCol_FrameBgActive] = ImVec4(0.03f, 0.03f, 0.04f, 0.67f);
    // colors[ImGuiCol_TitleBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
    // colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    // colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    // colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    // colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    // colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.17f, 0.17f, 1.00f);
    // colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    // colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
    // colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
    // colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.90f, 0.11f, 1.00f);
    // colors[ImGuiCol_Button] = ImVec4(0.21f, 0.22f, 0.23f, 0.40f);
    // colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
    // colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.55f, 0.55f, 1.00f);
    // colors[ImGuiCol_Header] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    // colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
    // colors[ImGuiCol_HeaderActive] = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
    // colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.16f, 0.50f);
    // colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    // colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    // colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    // colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    // colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    // colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.23f, 0.24f, 0.80f);
    // colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // // colors[ImGuiCol_TabSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // // colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.13f, 0.78f, 0.07f, 1.00f);
    // // colors[ImGuiCol_TabDimmed] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // // colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    // // colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.10f, 0.60f, 0.12f, 1.00f);
    // colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    // colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    // colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    // colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.14f, 0.87f, 0.05f, 1.00f);
    // colors[ImGuiCol_PlotHistogram] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
    // colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.23f, 0.78f, 0.02f, 1.00f);
    // colors[ImGuiCol_TableHeaderBg] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    // colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    // colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    // colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    // colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.46f, 0.47f, 0.46f, 0.06f);
    // // colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    // colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    // colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    // // colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    // colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    // colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.78f, 0.69f, 0.69f, 0.20f);
    // colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // style.WindowRounding = 4.0f;
    // style.FrameRounding = 4.0f;
    // style.GrabRounding = 3.0f;
    // style.PopupRounding = 4.0f;
    // style.TabRounding = 4.0f;
    // style.WindowMenuButtonPosition = ImGuiDir_Right;
    // style.ScrollbarSize = 10.0f;
    // style.GrabMinSize = 10.0f;
    // style.DockingSeparatorSize = 1.0f;
    // style.SeparatorTextBorderSize = 2.0f;
  }

  static auto _log_color(const spdlog::level::level_enum level) -> ImVec4 {
    switch (level) {
      case spdlog::level::trace:    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
      case spdlog::level::debug:    return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
      case spdlog::level::info:     return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
      case spdlog::level::warn:     return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
      case spdlog::level::err:      return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
      case spdlog::level::critical: return ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
      default:                      return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
  }

  static auto _lerp_color(const ImVec4& a, const ImVec4& b, std::float_t t) {
    return ImVec4{
      a.x + (b.x - a.x) * t,
      a.y + (b.y - a.y) * t,
      a.z + (b.z - a.z) * t,
      a.w + (b.w - a.w) * t
    };
  }

  static auto _get_color_for_time(const units::millisecond ms) -> ImVec4 {
    static constexpr auto green  = ImVec4{0.40f, 0.85f, 0.40f, 1.0f};
    static constexpr auto yellow = ImVec4{0.95f, 0.85f, 0.20f, 1.0f};
    static constexpr auto orange = ImVec4{0.95f, 0.55f, 0.15f, 1.0f};
    static constexpr auto red    = ImVec4{0.90f, 0.25f, 0.25f, 1.0f};

    if (ms <= 10.0f) {
      return green;
    } else if (ms <= 16.66f) {
      const auto t = (ms - 10.0f) / (16.66f - 10.0f);
      return _lerp_color(green, yellow, t);
    } else if (ms <= 33.33f) {
      const auto t = (ms - 16.66f) / (33.33f - 16.66f);
      return _lerp_color(yellow, red, t);
    } else {
      return red;
    }
  }

  auto _save() -> void {
    // [TODO] KAJ 2024-12-02 : Implement save
  }

  auto _load() -> void {
    // [TODO] KAJ 2024-12-02 : Implement load
  }

  auto _undo() -> void {
    // [TODO] KAJ 2024-12-02 : Implement undo
  }

  std::string _attachment_name;

  pipeline _pipeline;
  graphics::descriptor_handler _descriptor_handler;

  bool _show_demo_window;
  math::color _clear_color;

  utility::bit_field<popup> _open_popups;

  sbx::units::second _time;
  std::uint16_t _frames;
  std::uint16_t _fps;

  bool _has_auto_scroll;

  std::array<char, 32> _new_name_buffer;

  math::uuid _selected_node_id;

  math::vector2u _viewport_size;

  std::vector<std::float_t> _deltas;
  std::vector<std::float_t> _time_stamps;
  sbx::units::second _elapsed;

  std::vector<editor::menu> _menu;

  editor::themes _editor_theme;
  editor::fonts _editor_font;

}; // class editor_subrenderer

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_EDITOR_SUBRENDERER_HPP_
