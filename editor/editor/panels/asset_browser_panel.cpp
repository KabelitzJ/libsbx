// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>

namespace editor {

// Single source of truth for both classify_extension (below) and importable_extensions, so the
// "Import Asset..." file dialog's filter (see asset_browser_panel::draw) can never drift from
// what a dropped-in file would actually be classified as.
auto extension_table() -> const std::unordered_map<std::string, asset_kind>& {
  static const auto table = std::unordered_map<std::string, asset_kind>{
    {".png", asset_kind::texture},
    {".jpg", asset_kind::texture},
    {".jpeg", asset_kind::texture},
    {".gltf", asset_kind::mesh},
    {".glb", asset_kind::mesh},
    {".material", asset_kind::material},
    {".hdr", asset_kind::environment_map},
    {".particle_effect", asset_kind::particle_effect},
    {".animation_graph", asset_kind::animation_graph},
    {".yaml", asset_kind::scene},
    {".cs", asset_kind::script},
  };

  return table;
}

[[nodiscard]] auto is_importable_kind(asset_kind kind) -> bool {
  return kind == asset_kind::texture || kind == asset_kind::mesh ||
         kind == asset_kind::material || kind == asset_kind::environment_map ||
         kind == asset_kind::particle_effect || kind == asset_kind::animation_graph;
}

// Case-insensitive alphabetical order, shared by the folder tree and the contents pane so both
// panes read as "sorted" the same way.
[[nodiscard]] auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
  const auto to_lower = [](const std::filesystem::path& path) {
    auto name = path.filename().string();
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
  };

  return to_lower(lhs) < to_lower(rhs);
}

auto classify_extension(const std::filesystem::path& extension) -> asset_kind {
  const auto& table = extension_table();
  const auto entry = table.find(extension.string());

  return entry != table.end() ? entry->second : asset_kind::unknown;
}

/** @brief Every extension "Import Asset..."'s file dialog should offer — everything classify_extension routes through assets_module::import (i.e. every importable kind; .yaml/scene is excluded, same as the per-entry Import path). */
auto importable_extensions() -> std::vector<std::string> {
  auto extensions = std::vector<std::string>{};

  for (const auto& [extension, kind] : extension_table()) {
    if (is_importable_kind(kind)) {
      extensions.push_back(extension);
    }
  }

  return extensions;
}

auto icon_for(const asset_browser_entry& entry) -> const char* {
  if (entry.is_directory) {
    return ICON_MDI_FOLDER;
  }

  switch (entry.kind) {
    case asset_kind::texture: return ICON_MDI_IMAGE;
    case asset_kind::mesh: return ICON_MDI_CUBE_OUTLINE;
    case asset_kind::material: return ICON_MDI_PALETTE_SWATCH;
    case asset_kind::environment_map: return ICON_MDI_EARTH;
    case asset_kind::particle_effect: return ICON_MDI_FIREWORK;
    case asset_kind::animation_graph: return ICON_MDI_STATE_MACHINE;
    case asset_kind::scene: return ICON_MDI_FILE_TREE;
    case asset_kind::script: return ICON_MDI_FILE_CODE_OUTLINE;
    case asset_kind::unknown: return ICON_MDI_FILE_OUTLINE;
  }

  return ICON_MDI_FILE_OUTLINE;
}

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool {
  const auto* selected = std::get_if<asset_selection>(&state.current_selection);
  return selected != nullptr && selected->path == path;
}

// Which drag_drop_payload_* (asset_tile.hpp) a tile of this kind carries -- nullptr for kinds no
// Inspector picker ever accepts (environment_map, scene, script), which just aren't draggable.
auto drag_payload_type_for(asset_kind kind) -> const char* {
  switch (kind) {
    case asset_kind::texture: return sbx::render::widgets::drag_drop_payload_texture;
    case asset_kind::mesh: return sbx::render::widgets::drag_drop_payload_mesh;
    case asset_kind::material: return sbx::render::widgets::drag_drop_payload_material;
    case asset_kind::particle_effect: return sbx::render::widgets::drag_drop_payload_particle_effect;
    case asset_kind::animation_graph: return sbx::render::widgets::drag_drop_payload_animation_graph;
    default: return nullptr;
  }
}

// Truncates (with an ellipsis) rather than wrapping, so a long filename never grows a grid row
// taller than the fixed height ImGuiListClipper below assumes, and never overflows sideways into
// the next tile's column.
auto truncate_to_width(const std::string& text, std::float_t max_width) -> std::string {
  if (ImGui::CalcTextSize(text.c_str()).x <= max_width) {
    return text;
  }

  static constexpr auto ellipsis = std::string_view{"..."};

  auto truncated = text;

  while (!truncated.empty() && ImGui::CalcTextSize((truncated + std::string{ellipsis}).c_str()).x > max_width) {
    truncated.pop_back();
  }

  return truncated + std::string{ellipsis};
}

// Internal/tooling entries the Asset Browser should never surface, regardless of the search
// filter: .meta sidecars (asset_cooker's per-source uuid, not content of their own), the IDE-only
// Game.csproj the engine regenerates every start (see script_compiler::_write_ide_project) purely
// for IntelliSense, its bin/obj build output, and any dotfile/hidden entry (.git, .vs, .vscode, ...).
auto is_hidden_from_browser(const std::filesystem::path& name, bool is_directory) -> bool {
  const auto name_string = name.string();
  const auto name_extension = name.extension();

  if (!name_string.empty() && name_string.front() == '.') {
    return true;
  }

  if (is_directory) {
    return name_string == "bin" || name_string == "obj";
  }

  return name_extension == ".meta" || name_extension == ".csproj";
}

// Whether `directory` contains at least one subdirectory the tree would actually show -- used to
// suppress the expand arrow on leaf directories, since there's nothing for it to expand into.
[[nodiscard]] auto has_visible_subdirectories(const std::filesystem::path& directory) -> bool {
  auto ec = std::error_code{};

  for (const auto& entry : std::filesystem::directory_iterator{directory, ec}) {
    if (entry.is_directory(ec) && !is_hidden_from_browser(entry.path().filename(), true)) {
      return true;
    }
  }

  return false;
}

// True if `candidate` is `target` itself or one of its ancestors (path-component prefix) -- used
// to decide which nodes along a "reveal" target's chain need to be forced open.
[[nodiscard]] auto is_ancestor_or_self(const std::filesystem::path& candidate, const std::filesystem::path& target) -> bool {
  auto candidate_it = candidate.begin();
  auto target_it = target.begin();

  for (; candidate_it != candidate.end(); ++candidate_it, ++target_it) {
    if (target_it == target.end() || *candidate_it != *target_it) {
      return false;
    }
  }

  return true;
}

// Case-insensitive substring test for the search box -- an empty filter matches everything.
auto filter_matches(std::string_view name, std::string_view filter) -> bool {
  if (filter.empty()) {
    return true;
  }

  const auto to_lower = [](std::string_view text) {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(name).find(to_lower(filter)) != std::string::npos;
}

auto asset_browser_panel::_refresh_entries() -> void {
  _cached_entries.clear();

  auto& project = sbx::core::engine::project();
  const auto absolute_directory = project.assets_directory() / _current_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    _needs_refresh = false;
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    if (is_hidden_from_browser(dir_entry.path().filename(), dir_entry.is_directory())) {
      continue;
    }

    auto entry = asset_browser_entry{};
    entry.path = _current_directory / dir_entry.path().filename();
    entry.is_directory = dir_entry.is_directory();

    if (!entry.is_directory) {
      entry.kind = classify_extension(dir_entry.path().extension());
      entry.is_importable = is_importable_kind(entry.kind);
    }

    _cached_entries.push_back(std::move(entry));
  }

  std::ranges::sort(_cached_entries, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory > rhs.is_directory;
    }

    return filename_less(lhs.path, rhs.path);
  });

  _needs_refresh = false;
}

// Recursively lists subdirectories only, live per expanded node — cheap (names only, no imports).
auto asset_browser_panel::_draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void {
  const auto absolute_directory = absolute_assets_root / relative_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    return;
  }

  auto subdirectories = std::vector<std::filesystem::path>{};

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    if (dir_entry.is_directory() && !is_hidden_from_browser(dir_entry.path().filename(), true)) {
      subdirectories.push_back(dir_entry.path());
    }
  }

  std::ranges::sort(subdirectories, filename_less);

  for (const auto& subdirectory : subdirectories) {
    const auto relative_child = relative_directory / subdirectory.filename();
    const auto child_name = subdirectory.filename().string();
    const auto is_leaf = !has_visible_subdirectories(subdirectory);

    auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (_current_directory == relative_child) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (is_leaf) {
      // Nothing to expand into -- no arrow, no toggle, and no separate TreePop (it's pushed and
      // popped in one go by TreeNodeEx itself when this flag is set).
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    ImGui::PushID(relative_child.string().c_str());

    if (_pending_reveal && is_ancestor_or_self(relative_child, *_pending_reveal)) {
      ImGui::SetNextItemOpen(true);
    }

    const auto is_open = ImGui::TreeNodeEx("##dir", flags, "%s %s", ICON_MDI_FOLDER, child_name.c_str());

    if (_pending_reveal && relative_child == *_pending_reveal) {
      ImGui::SetScrollHereY(0.5f);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _navigate_to(relative_child);
    }

    if (is_open && !is_leaf) {
      _draw_directory_tree(state, absolute_assets_root, relative_child);
      ImGui::TreePop();
    }

    ImGui::PopID();
  }
}

auto asset_browser_panel::draw(editor_state& state) -> void {
  // The two panes below scroll on their own (see panes_height) — the panel itself never needs to.
  ImGui::Begin(window_name, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  // "Show in Browser" (Inspector's picker slots / asset properties view) -- one-shot, unlike
  // editor_state::current_selection, so it never fights the user for navigating this panel on
  // their own afterward.
  if (state.reveal_in_browser_request) {
    _navigate_to(state.reveal_in_browser_request->parent_path());
    state.reveal_in_browser_request.reset();
  }

  if (auto picked = _import_dialog.result()) {
    _pending_asset_imports.insert(_pending_asset_imports.end(), picked->begin(), picked->end());
  }

  _process_pending_asset_imports(state);

  if (ImGui::Button(ICON_MDI_FILE_IMPORT " Import Asset...")) {
    _import_dialog.open("Import Asset", sbx::render::widgets::file_dialog_mode::open_files, project.assets_directory() / _current_directory, importable_extensions());
  }

  ImGui::SameLine();

  if (ImGui::Button("Import All in This Folder")) {
    // import_directory (like import()) needs a path resolvable from cwd, not one merely
    // relative to assets_directory() — see assets_module.hpp's doc comment.
    assets_module.import_directory(project.assets_directory() / _current_directory);
    _needs_refresh = true;
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_PLUS " New Material")) {
    auto file_name = std::string{"New Material.material"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("New Material {}.material", suffix++);
    }

    const auto relative_path = _current_directory / file_name;

    auto handle = assets_module.create_material(sbx::assets::material::create_info{.name = "New Material"});
    const auto id = assets_module.save_material(handle, relative_path);

    _needs_refresh = true;
    state.select_asset(id, relative_path, asset_kind::material);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_FIREWORK " New Particle Effect")) {
    auto file_name = std::string{"New Particle Effect.particle_effect"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("New Particle Effect {}.particle_effect", suffix++);
    }

    const auto relative_path = _current_directory / file_name;

    auto handle = assets_module.create_particle_effect(sbx::assets::particle_effect::create_info{.name = "New Particle Effect"});
    const auto id = assets_module.save_particle_effect(handle, relative_path);

    _needs_refresh = true;
    state.select_asset(id, relative_path, asset_kind::particle_effect);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_STATE_MACHINE " New Animation Graph")) {
    auto file_name = std::string{"New Animation Graph.animation_graph"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("New Animation Graph {}.animation_graph", suffix++);
    }

    const auto relative_path = _current_directory / file_name;

    // A single entry state so the graph is already is_valid() -- states/transitions beyond this
    // are hand-authored in the saved .animation_graph file until the visual graph editor lands
    // (see the animator's Inspector section, which only edits parameters, not graph structure).
    auto create_info = sbx::assets::animation_graph::create_info{.name = "New Animation Graph"};
    create_info.states.push_back(sbx::assets::animation_state{.id = 0u, .name = "Idle"});
    create_info.entry_state_id = 0u;

    auto handle = assets_module.create_animation_graph(create_info);
    const auto id = assets_module.save_animation_graph(handle, relative_path);

    _needs_refresh = true;
    state.select_asset(id, relative_path, asset_kind::animation_graph);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_FILE_CODE_OUTLINE " New Script")) {
    auto file_name = std::string{"NewScript.cs"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("NewScript{}.cs", suffix++);
    }

    const auto relative_path = _current_directory / file_name;
    const auto absolute_path = project.assets_directory() / relative_path;
    const auto class_name = std::filesystem::path{file_name}.stem().string();

    std::filesystem::create_directories(absolute_path.parent_path());

    auto out = std::ofstream{absolute_path};
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

    _needs_refresh = true;
    state.select_asset(sbx::math::uuid::nil(), relative_path, asset_kind::script);
  }

  ImGui::Separator();

  // Breadcrumb bar: "assets" root button, then one clickable button per path segment. Iterates a
  // snapshot of the path rather than _current_directory itself, since a segment's own click below
  // reassigns _current_directory mid-loop (via _navigate_to), which would otherwise invalidate
  // this loop's iterators.
  if (ImGui::Button(ICON_MDI_FOLDER_OPEN " assets")) {
    _navigate_to(std::filesystem::path{});
  }

  const auto breadcrumb_directory = _current_directory;
  auto breadcrumb_path = std::filesystem::path{};

  for (const auto& segment : breadcrumb_directory) {
    breadcrumb_path /= segment;

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextDisabled("%s", ICON_MDI_CHEVRON_RIGHT);
    ImGui::SameLine(0.0f, 4.0f);

    ImGui::PushID(breadcrumb_path.string().c_str());

    if (ImGui::Button(segment.string().c_str())) {
      _navigate_to(breadcrumb_path);
    }

    ImGui::PopID();
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(160.0f);
  ImGui::InputTextWithHint("##asset_browser_search", ICON_MDI_MAGNIFY " Search...", _search_filter.data(), _search_filter.size());

  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::SliderFloat("##asset_browser_tile_size", &_tile_size, 48.0f, 128.0f, "%.0f px");

  if (_needs_refresh) {
    _refresh_entries();
  }

  // Space left below the toolbar, minus the table's own per-cell padding (added around each
  // child below on top of whatever height we give it) — both panes get exactly this height, so
  // the table's one row never grows past what's actually left and the panel never overflows.
  const auto panes_height = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().CellPadding.y * 2.0f;

  // Folder tree gets a narrow fixed-width column (user-resizable); contents gets the rest.
  if (ImGui::BeginTable("asset_browser_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##asset_browser_tree_scroll", ImVec2(0.0f, panes_height));

    const auto root_is_leaf = !has_visible_subdirectories(project.assets_directory());

    auto root_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (_current_directory.empty()) {
      root_flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (root_is_leaf) {
      root_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Any reveal target lives somewhere under the root by definition.
    if (_pending_reveal) {
      ImGui::SetNextItemOpen(true);
    }

    const auto is_root_open = ImGui::TreeNodeEx("##assets_root", root_flags, "%s assets", ICON_MDI_FOLDER_OPEN);

    if (_pending_reveal && _pending_reveal->empty()) {
      ImGui::SetScrollHereY(0.5f);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _navigate_to(std::filesystem::path{});
    }

    if (is_root_open && !root_is_leaf) {
      _draw_directory_tree(state, project.assets_directory(), std::filesystem::path{});
      ImGui::TreePop();
    }

    // Consumed for exactly the one frame the reveal target's chain needed forcing open -- lets
    // the tree be collapsed by hand afterward instead of snapping back open every frame.
    _pending_reveal.reset();

    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("##asset_browser_contents_scroll", ImVec2(0.0f, panes_height));

    // Entries matching the search box, keeping _cached_entries' existing order (directories
    // first, then case-insensitive alphabetical).
    auto visible = std::vector<std::size_t>{};

    for (auto index = std::size_t{0u}; index < _cached_entries.size(); ++index) {
      if (filter_matches(_cached_entries[index].path.filename().string(), _search_filter.data())) {
        visible.push_back(index);
      }
    }

    if (visible.empty()) {
      ImGui::TextDisabled("Nothing here.");
    }

    const auto avail_width = ImGui::GetContentRegionAvail().x;
    const auto cell_width = _tile_size + 8.0f;
    const auto columns = std::max(std::int32_t{1}, static_cast<std::int32_t>(avail_width / cell_width));

    const auto row_height = _tile_size + ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;
    const auto row_count = (static_cast<std::int32_t>(visible.size()) + columns - 1) / columns;

    // Clipped by grid row so a folder full of textures only ever loads/thumbnails the tiles
    // actually on screen, instead of every texture in it every frame the panel is open.
    auto clipper = ImGuiListClipper{};
    clipper.Begin(row_count, row_height);

    while (clipper.Step()) {
      for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        for (auto column = std::int32_t{0}; column < columns; ++column) {
          const auto visible_index = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column);

          if (visible_index >= visible.size()) {
            break;
          }

          if (column > 0) {
            ImGui::SameLine();
          }

          auto& entry = _cached_entries[visible[visible_index]];

          ImGui::PushID(entry.path.string().c_str());
          ImGui::BeginGroup();

          auto tile_desc = sbx::render::widgets::asset_tile_desc{};
          tile_desc.icon_glyph = icon_for(entry);
          tile_desc.is_directory = entry.is_directory;
          tile_desc.is_selected = is_entry_selected(state, entry.path);
          tile_desc.size = ImVec2{_tile_size, _tile_size};
          tile_desc.display_name = entry.path.filename().string();
          tile_desc.drag_payload_type = drag_payload_type_for(entry.kind);
          tile_desc.drag_id = entry.id;
          tile_desc.drag_path = entry.path;

          if (entry.kind == asset_kind::texture) {
            tile_desc.is_texture_thumbnail = true;
            tile_desc.texture = assets_module.load_texture(entry.path);
          }

          const auto tile_result = sbx::render::widgets::draw_asset_tile("##tile", tile_desc);

          const auto label = truncate_to_width(entry.path.filename().string(), _tile_size);
          const auto label_width = ImGui::CalcTextSize(label.c_str()).x;
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (_tile_size - label_width) * 0.5f));
          ImGui::TextUnformatted(label.c_str());

          ImGui::EndGroup();

          if (tile_result.hovered) {
            ImGui::SetTooltip("%s", entry.path.string().c_str());
          }

          if (tile_result.clicked) {
            if (entry.is_directory) {
              _navigate_to(entry.path);
            } else if (entry.is_importable) {
              const auto meta_path = std::filesystem::path{project.assets_directory() / entry.path}.concat(".meta");

              if (entry.kind == asset_kind::mesh && !std::filesystem::exists(meta_path)) {
                // First time this mesh has ever been seen — let the user decide whether to extract
                // its materials before it's actually cooked (deferring would mean the choice has
                // nowhere to be remembered until something else needs the mesh).
                _pending_import_path = entry.path;
                _import_extract_materials = true;
                _show_import_mesh_dialog = true;
              } else {
                // Same resolution requirement as above — entry.path is relative to assets_directory().
                entry.id = assets_module.import(project.assets_directory() / entry.path);
                state.select_asset(entry.id, entry.path, entry.kind);
              }
            } else if (entry.kind == asset_kind::scene) {
              state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::scene);
            } else if (entry.kind == asset_kind::script) {
              state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::script);
            }

            // A regular click already ran above (double_clicked implies clicked -- see
            // asset_tile.cpp), so entry.id is already resolved by the importable-kind branch.
            if (tile_result.double_clicked && entry.kind == asset_kind::animation_graph) {
              state.request_open_animation_graph_editor(entry.id, entry.path);
            }
          }

          ImGui::PopID();
        }
      }
    }

    // Right-click the empty area of the contents pane (not an entry — see NoOpenOverItems) for
    // the same "Import Asset..." action as the toolbar button above.
    if (ImGui::BeginPopupContextWindow("##asset_browser_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      if (ImGui::MenuItem(ICON_MDI_FILE_IMPORT " Import Asset...")) {
        _import_dialog.open("Import Asset", sbx::render::widgets::file_dialog_mode::open_files, project.assets_directory() / _current_directory, importable_extensions());
      }

      ImGui::EndPopup();
    }

    ImGui::EndChild();

    ImGui::EndTable();
  }

  if (_show_import_mesh_dialog) {
    ImGui::OpenPopup("Import Mesh");
    _show_import_mesh_dialog = false;
  }

  if (ImGui::BeginPopupModal("Import Mesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Import '%s'", _pending_import_path.filename().string().c_str());
    ImGui::Checkbox("Extract materials to editable .material assets", &_import_extract_materials);
    ImGui::TextDisabled("Recommended. When off, materials are cooked read-only and won't appear in the Asset Browser.");

    if (ImGui::Button("Import")) {
      const auto id = assets_module.import(project.assets_directory() / _pending_import_path);
      assets_module.load_mesh(id, sbx::assets::mesh_import_options{.extract_materials = _import_extract_materials});
      state.select_asset(id, _pending_import_path, asset_kind::mesh);
      _needs_refresh = true; // newly extracted .material files may now be visible in this folder
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  _import_dialog.draw();

  if (_show_import_conflict_dialog) {
    ImGui::OpenPopup("Import Conflict");
    _show_import_conflict_dialog = false;
  }

  if (ImGui::BeginPopupModal("Import Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("'%s' already exists in this folder.", _import_conflict_destination.filename().string().c_str());

    if (ImGui::Button("Overwrite")) {
      _import_asset_file(state, _import_conflict_source, _import_conflict_destination);
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Skip")) {
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel Remaining")) {
      _pending_asset_imports.clear();
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::End();
}

auto asset_browser_panel::_process_pending_asset_imports(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();

  while (!_pending_asset_imports.empty() && !_import_conflict_unresolved) {
    const auto source = _pending_asset_imports.front();
    _pending_asset_imports.erase(_pending_asset_imports.begin());

    const auto destination = project.assets_directory() / _current_directory / source.filename();

    if (std::filesystem::exists(destination)) {
      _import_conflict_source = source;
      _import_conflict_destination = destination;
      _import_conflict_unresolved = true;
      _show_import_conflict_dialog = true;

      break;
    }

    _import_asset_file(state, source, destination);
  }
}

auto asset_browser_panel::_import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  std::filesystem::create_directories(destination.parent_path());

  // copy_file throws if source and destination are the same file (e.g. picking a file already
  // inside the current folder via the dialog) — nothing to copy in that case, just (re-)import it.
  auto ec = std::error_code{};
  if (!std::filesystem::equivalent(source, destination, ec)) {
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
  }

  const auto relative_path = std::filesystem::relative(destination, project.assets_directory());
  const auto kind = classify_extension(destination.extension());

  if (kind == asset_kind::mesh && !std::filesystem::exists(std::filesystem::path{destination}.concat(".meta"))) {
    // First time this mesh has ever been seen — same "let the user choose extract_materials"
    // detour the per-entry Import path takes, and the same modal handles both (see draw()).
    _pending_import_path = relative_path;
    _import_extract_materials = true;
    _show_import_mesh_dialog = true;
  } else {
    const auto id = assets_module.import(destination);
    state.select_asset(id, relative_path, kind);
  }

  _needs_refresh = true;
}

} // namespace editor
