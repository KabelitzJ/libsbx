// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/shader_graph.hpp>

namespace editor {

auto asset_browser_panel::_create_material(editor_state& state, const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto relative_path = target_directory / unique_name(absolute_directory, "New Material", ".material");

  auto handle = assets_module.create_material(sbx::assets::material::create_info{.name = "New Material"});
  const auto id = assets_module.save_material(handle, relative_path);

  _navigate_to(target_directory);
  state.select_asset(id, relative_path, asset_kind::material);
}

auto asset_browser_panel::_create_particle_effect(editor_state& state, const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto relative_path = target_directory / unique_name(absolute_directory, "New Particle Effect", ".particle_effect");

  auto handle = assets_module.create_particle_effect(sbx::assets::particle_effect::create_info{.name = "New Particle Effect"});
  const auto id = assets_module.save_particle_effect(handle, relative_path);

  _navigate_to(target_directory);
  state.select_asset(id, relative_path, asset_kind::particle_effect);
}

auto asset_browser_panel::_create_animation_graph(editor_state& state, const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto relative_path = target_directory / unique_name(absolute_directory, "New Animation Graph", ".animation_graph");

  // A single entry state so the graph is already is_valid() -- states/transitions beyond this
  // are hand-authored in the saved .animation_graph file until the visual graph editor lands
  // (see the animator's Inspector section, which only edits parameters, not graph structure).
  auto create_info = sbx::assets::animation_graph::create_info{.name = "New Animation Graph"};
  create_info.states.push_back(sbx::assets::animation_state{.id = 0u, .name = "Idle"});
  create_info.entry_state_id = 0u;

  auto handle = assets_module.create_animation_graph(create_info);
  const auto id = assets_module.save_animation_graph(handle, relative_path);

  _navigate_to(target_directory);
  state.select_asset(id, relative_path, asset_kind::animation_graph);
}

auto asset_browser_panel::_create_shader_graph(editor_state& state, const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto relative_path = target_directory / unique_name(absolute_directory, "New Shader Graph", ".shadergraph");

  // A flat mid-gray feeding Direct Output -- degenerate (every light contributes the same flat
  // color) but cook-clean and is_valid() immediately, same reasoning _create_animation_graph's
  // single Idle state gives a fresh animation graph. Real content is the point of the graph editor
  // this creates, not this seed.
  auto create_info = sbx::assets::shader_graph::create_info{.name = "New Shader Graph"};

  auto color_node = sbx::assets::shader_graph_node{};
  color_node.id = 0u;
  color_node.type = sbx::assets::shader_node_type::constant_color;
  color_node.editor_position = sbx::math::vector2{0.0f, 0.0f};
  color_node.value = sbx::math::color{0.5f, 0.5f, 0.5f, 1.0f};

  auto output_node = sbx::assets::shader_graph_node{};
  output_node.id = 1u;
  output_node.type = sbx::assets::shader_node_type::output_direct;
  output_node.editor_position = sbx::math::vector2{300.0f, 0.0f};

  create_info.nodes = {color_node, output_node};
  create_info.edges = {sbx::assets::shader_graph_edge{.from_node = 0u, .from_pin = 0u, .to_node = 1u, .to_pin = 0u}};

  auto handle = assets_module.create_shader_graph(create_info);
  const auto id = assets_module.save_shader_graph(handle, relative_path);

  _navigate_to(target_directory);
  state.select_asset(id, relative_path, asset_kind::shader_graph);
}

auto asset_browser_panel::_create_script(editor_state& state, const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto file_name = unique_name(absolute_directory, "NewScript", ".cs");
  const auto relative_path = target_directory / file_name;
  const auto class_name = std::filesystem::path{file_name}.stem().string();

  auto out = std::ofstream{absolute_directory / file_name};
  out << fmt::format(
    "using Sbx.Core;\n\n"
    "public class {} : Behavior\n"
    "{{\n"
    "    public override void OnCreate()\n"
    "    {{\n"
    "    }}\n\n"
    "    public override void OnUpdate()\n"
    "    {{\n"
    "    }}\n\n"
    "    public override void OnDestroy()\n"
    "    {{\n"
    "    }}\n"
    "}}\n",
    class_name
  );

  _navigate_to(target_directory);
  state.select_asset(sbx::math::uuid::nil(), relative_path, asset_kind::script);
}

auto asset_browser_panel::_create_folder(const std::filesystem::path& target_directory) -> void {
  auto& project = sbx::core::engine::project();

  const auto absolute_directory = project.assets_directory() / target_directory;
  std::filesystem::create_directories(absolute_directory);

  const auto relative_path = target_directory / unique_name(absolute_directory, "New Folder", "");
  std::filesystem::create_directory(project.assets_directory() / relative_path);

  _navigate_to(target_directory);
  _begin_rename(relative_path, true); // Explorer/Unity both drop straight into rename on create
}

auto asset_browser_panel::_draw_create_menu(editor_state& state, const std::filesystem::path& target_directory) -> void {
  if (ImGui::MenuItem(ICON_MDI_PALETTE_SWATCH " Material")) {
    _create_material(state, target_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_FIREWORK " Particle Effect")) {
    _create_particle_effect(state, target_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_STATE_MACHINE " Animation Graph")) {
    _create_animation_graph(state, target_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_VECTOR_POLYLINE " Shader Graph")) {
    _create_shader_graph(state, target_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_FILE_CODE_OUTLINE " C# Script")) {
    _create_script(state, target_directory);
  }

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_MDI_FOLDER_PLUS " New Folder")) {
    _create_folder(target_directory);
  }

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_MDI_FILE_IMPORT " Import from Disk...")) {
    auto& project = sbx::core::engine::project();
    _import_destination_directory = target_directory;
    _import_dialog.open({
      .title = "Import Asset",
      .mode = sbx::render::file_dialog_mode::open_files,
      .start_dir = project.assets_directory() / target_directory,
      .extensions = importable_extensions(),
      .shortcuts = {{.label = "Assets", .path = project.assets_directory()}},
      .confirm_label = "Import",
    });
  }

  if (ImGui::MenuItem(ICON_MDI_REFRESH " Reimport All in This Folder")) {
    auto& project = sbx::core::engine::project();
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    // import_directory (like import()) needs a path resolvable from cwd, not one merely
    // relative to assets_directory() — see assets_module.hpp's doc comment.
    assets_module.import_directory(project.assets_directory() / target_directory);
    _needs_refresh = true;
  }
}

auto asset_browser_panel::_duplicate(editor_state& state, const std::filesystem::path& relative_path, bool is_directory) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_directory = project.assets_directory() / relative_path.parent_path();
  const auto stem = is_directory ? relative_path.filename().string() : relative_path.stem().string();
  const auto extension = is_directory ? std::string{} : relative_path.extension().string();

  const auto new_name = unique_name(absolute_directory, stem + " Copy", extension);
  const auto source_absolute = project.assets_directory() / relative_path;
  const auto destination_absolute = absolute_directory / new_name;

  auto ec = std::error_code{};

  if (is_directory) {
    // .meta files inside are intentionally copied too and then stripped below, so the duplicate's
    // assets mint fresh uuids on next import instead of sharing identity with the originals.
    std::filesystem::copy(source_absolute, destination_absolute, std::filesystem::copy_options::recursive, ec);

    if (!ec) {
      for (const auto& sub_entry : std::filesystem::recursive_directory_iterator{destination_absolute}) {
        if (sub_entry.path().extension() == ".meta") {
          std::filesystem::remove(sub_entry.path());
        }
      }
    }
  } else {
    std::filesystem::copy_file(source_absolute, destination_absolute, ec);
  }

  if (ec) {
    return;
  }

  _needs_refresh = true;

  const auto new_relative = relative_path.parent_path() / new_name;

  if (!is_directory) {
    if (const auto kind = classify_extension(destination_absolute.extension()); is_importable_kind(kind)) {
      const auto id = assets_module.import(destination_absolute);
      state.select_asset(id, new_relative, kind);
    }
  }
}

auto asset_browser_panel::_request_delete(const std::filesystem::path& relative_path, bool is_directory) -> void {
  _pending_delete_path = relative_path;
  _pending_delete_is_directory = is_directory;
  _show_delete_confirm_dialog = true;
}

auto asset_browser_panel::_try_move(editor_state& state, const std::filesystem::path& source_relative, const std::filesystem::path& destination_directory_relative) -> void {
  if (source_relative.empty() || source_relative.parent_path() == destination_directory_relative) {
    return; // already there
  }

  // Refuses moving a folder onto itself or into one of its own descendants (is_ancestor_or_self
  // with candidate=source catches both: destination == source, and destination under source/).
  if (is_ancestor_or_self(source_relative, destination_directory_relative)) {
    return;
  }

  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto source_absolute = project.assets_directory() / source_relative;
  const auto new_relative = destination_directory_relative / source_relative.filename();
  const auto destination_absolute = project.assets_directory() / new_relative;

  if (std::filesystem::exists(destination_absolute)) {
    return; // name clash in the target folder -- silently refuse, same spirit as rename
  }

  if (!assets_module.move_asset(source_absolute, destination_absolute)) {
    return;
  }

  _needs_refresh = true;

  if (const auto* selected = std::get_if<asset_selection>(&state.current_selection); selected != nullptr && selected->path == source_relative) {
    state.select_asset(selected->id, new_relative, selected->kind);
  }
}

} // namespace editor
