// SPDX-License-Identifier: MIT
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <system_error>

#include <fmt/format.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

constexpr auto image_extensions = std::array<std::string_view, 2>{".png", ".jpg"};
constexpr auto mesh_extensions = std::array<std::string_view, 1>{".gltf"};

auto _is_image_extension(std::string_view extension) -> bool {
  return std::ranges::find(image_extensions, extension) != image_extensions.end();
}

auto _is_mesh_extension(std::string_view extension) -> bool {
  return std::ranges::find(mesh_extensions, extension) != mesh_extensions.end();
}

auto _canonical_string(const std::filesystem::path& path) -> std::string {
  auto error = std::error_code{};
  auto canonical = std::filesystem::weakly_canonical(path, error);

  if (error) {
    return path.lexically_normal().generic_string();
  }

  return canonical.generic_string();
}

auto _matches_filter(std::string_view name, std::string_view filter) -> bool {
  if (filter.empty()) {
    return true;
  }

  return name.find(filter) != std::string_view::npos;
}

auto asset_browser_panel::draw() -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (_root.empty()) {
    _root = assets_module.asset_root();
    _selected_directory = _root;
  }

  _refresh_image_cache();

  ImGui::Begin(ICON_MDI_FOLDER_MULTIPLE_IMAGE " Assets###asset_browser_panel");

  if (ImGui::Button(ICON_MDI_REFRESH " Refresh")) {
    _last_image_count = 0;
    _path_to_image.clear();
  }

  ImGui::SameLine();

  ImGui::SetNextItemWidth(-FLT_MIN);

  _search_buffer.resize(256);
  ImGui::InputTextWithHint("##search", ICON_MDI_MAGNIFY " Search", _search_buffer.data(), _search_buffer.capacity());

  if (ImGui::BeginTabBar("##asset_filter_tabs")) {
    if (ImGui::BeginTabItem(ICON_MDI_FILE_MULTIPLE " All")) {
      _active_filter = filter_kind::all;
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(ICON_MDI_IMAGE " Images")) {
      _active_filter = filter_kind::images;
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(ICON_MDI_CUBE_OUTLINE " Meshes")) {
      _active_filter = filter_kind::meshes;
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::Separator();

  const auto avail = ImGui::GetContentRegionAvail();

  if (_tree_width <= 0.0f) {
    _tree_width = std::max(180.0f, avail.x * 0.25f);
  }

  const auto min_pane = 120.0f;
  const auto splitter_thickness = 4.0f;

  auto tree_width = _tree_width;
  auto content_width = std::max(min_pane, avail.x - tree_width - splitter_thickness);

  ImGui::BeginChild("##tree", ImVec2{tree_width, 0.0f}, ImGuiChildFlags_Borders);
  _draw_tree(_root);
  ImGui::EndChild();

  ImGui::SameLine(0.0f, 0.0f);

  ImGui::InvisibleButton("##splitter", ImVec2{splitter_thickness, std::max(1.0f, avail.y)});

  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }

  if (ImGui::IsItemActive()) {
    _tree_width = std::clamp(_tree_width + ImGui::GetIO().MouseDelta.x, min_pane, avail.x - min_pane - splitter_thickness);
  }

  ImGui::SameLine(0.0f, 0.0f);

  ImGui::BeginChild("##content", ImVec2{content_width, 0.0f}, ImGuiChildFlags_Borders);

  _draw_breadcrumb(_root);

  ImGui::Separator();

  ImGui::SliderFloat("##item_size", &_item_size, 48.0f, 192.0f, "Size: %.0f");

  ImGui::Separator();

  ImGui::BeginChild("##grid", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
  _draw_grid();
  ImGui::EndChild();

  ImGui::EndChild();

  ImGui::End();
}

auto asset_browser_panel::_draw_tree(const std::filesystem::path& directory) -> void {
  auto error = std::error_code{};

  auto subdirectories = std::vector<std::filesystem::directory_entry>{};

  for (const auto& entry : std::filesystem::directory_iterator{directory, error}) {
    if (entry.is_directory(error)) {
      subdirectories.push_back(entry);
    }
  }

  std::ranges::sort(subdirectories, [](const auto& lhs, const auto& rhs) {
    return lhs.path().filename() < rhs.path().filename();
  });

  const auto key = _canonical_string(directory);
  const auto is_expanded = _expanded_directories.contains(key);
  const auto is_selected = _canonical_string(_selected_directory) == key;
  const auto is_leaf = subdirectories.empty();

  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (is_selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  if (is_expanded) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  if (is_leaf) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
  }

  const auto label = directory == _root ? std::string{ICON_MDI_FOLDER_HOME " res://"} : fmt::format(ICON_MDI_FOLDER " {}", directory.filename().generic_string());

  const auto open = ImGui::TreeNodeEx(key.c_str(), flags, "%s", label.c_str());

  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    _selected_directory = directory;
  }

  if (is_leaf) {
    _expanded_directories.erase(key);
    return;
  }

  if (open) {
    _expanded_directories.insert(key);

    for (const auto& entry : subdirectories) {
      _draw_tree(entry.path());
    }

    ImGui::TreePop();
  } else {
    _expanded_directories.erase(key);
  }
}

auto asset_browser_panel::_draw_breadcrumb(const std::filesystem::path& root) -> void {
  auto error = std::error_code{};
  const auto relative = std::filesystem::relative(_selected_directory, root, error);

  if (ImGui::Button(ICON_MDI_FOLDER_HOME " res://")) {
    _selected_directory = root;
  }

  if (error || relative.empty() || relative == ".") {
    return;
  }

  auto accumulated = root;

  for (const auto& part : relative) {
    accumulated /= part;

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextDisabled("/");
    ImGui::SameLine(0.0f, 4.0f);

    if (ImGui::Button(part.generic_string().c_str())) {
      _selected_directory = accumulated;
    }
  }
}

auto asset_browser_panel::_draw_grid() -> void {
  auto error = std::error_code{};

  const auto filter = std::string{_search_buffer.c_str()};

  auto entries = std::vector<std::filesystem::directory_entry>{};

  for (const auto& entry : std::filesystem::directory_iterator{_selected_directory, error}) {
    entries.push_back(entry);
  }

  if (error) {
    ImGui::TextDisabled("Failed to read directory: %s", error.message().c_str());
    return;
  }

  std::ranges::sort(entries, [](const auto& lhs, const auto& rhs) {
    const auto lhs_dir = lhs.is_directory();
    const auto rhs_dir = rhs.is_directory();

    if (lhs_dir != rhs_dir) {
      return lhs_dir;
    }

    return lhs.path().filename() < rhs.path().filename();
  });

  const auto avail = ImGui::GetContentRegionAvail().x;
  const auto padding = 8.0f;
  const auto cell = _item_size + padding;
  const auto columns = std::max(1, static_cast<std::int32_t>(avail / cell));

  auto current_column = 0;
  auto index = 0;

  for (const auto& entry : entries) {
    const auto name = entry.path().filename().generic_string();

    if (!_matches_filter(name, filter)) {
      continue;
    }

    if (!entry.is_directory()) {
      auto extension = entry.path().extension().generic_string();

      std::ranges::transform(extension, extension.begin(), [](auto character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
      });

      const auto is_image = _is_image_extension(extension);
      const auto is_mesh = _is_mesh_extension(extension);

      switch (_active_filter) {
        case filter_kind::all: {
          if (!is_image && !is_mesh) {
            continue;
          }
          break;
        }
        case filter_kind::images: {
          if (!is_image) {
            continue;
          }
          break;
        }
        case filter_kind::meshes: {
          if (!is_mesh) {
            continue;
          }
          break;
        }
      }
    }

    ImGui::PushID(index++);

    _draw_file_card(entry);

    ImGui::PopID();

    ++current_column;

    if (current_column < columns) {
      ImGui::SameLine(0.0f, padding);
    } else {
      current_column = 0;
    }
  }
}

auto asset_browser_panel::_draw_file_card(const std::filesystem::directory_entry& entry) -> void {
  const auto& path = entry.path();
  const auto name = path.filename().generic_string();

  ImGui::BeginGroup();

  if (entry.is_directory()) {
    if (ImGui::Button(ICON_MDI_FOLDER "##dir", ImVec2{_item_size, _item_size})) {
      _selected_directory = path;
    }
  } else {
    auto extension = path.extension().generic_string();

    std::ranges::transform(extension, extension.begin(), [](auto character) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });

    if (_is_image_extension(extension)) {
      const auto canonical = _canonical_string(path);

      auto handle_entry = _path_to_image.find(canonical);
      auto descriptor_set = static_cast<VkDescriptorSet>(VK_NULL_HANDLE);
      auto handle = sbx::graphics::image2d_handle{};

      if (handle_entry != _path_to_image.end()) {
        handle = handle_entry->second;
        descriptor_set = _texture_cache.descriptor_for(handle);
      }

      if (descriptor_set != VK_NULL_HANDLE) {
        ImGui::ImageButton("##thumb", reinterpret_cast<ImTextureID>(descriptor_set), ImVec2{_item_size, _item_size});
      } else {
        ImGui::Button(ICON_MDI_IMAGE "##img", ImVec2{_item_size, _item_size});
      }

      if (handle.is_valid() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(payload_image_handle, &handle, sizeof(sbx::graphics::image2d_handle));
        ImGui::Text("%s", name.c_str());
        ImGui::EndDragDropSource();
      }
    } else if (_is_mesh_extension(extension)) {
      ImGui::Button(ICON_MDI_CUBE_OUTLINE "##mesh", ImVec2{_item_size, _item_size});
    } else {
      ImGui::Button(ICON_MDI_FILE_OUTLINE "##file", ImVec2{_item_size, _item_size});
    }
  }

  ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + _item_size);
  ImGui::TextWrapped("%s", name.c_str());
  ImGui::PopTextWrapPos();

  ImGui::EndGroup();
}

auto asset_browser_panel::_refresh_image_cache() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto& asset_registry = scenes_module.asset_registry();
  const auto current_count = asset_registry.images().size();

  if (current_count == _last_image_count && !_path_to_image.empty()) {
    return;
  }

  _path_to_image.clear();

  for (const auto& [handle, metadata] : asset_registry.images()) {
    if (metadata.path.empty()) {
      continue;
    }

    const auto native = assets_module.resolve_path(metadata.path);
    _path_to_image.emplace(_canonical_string(native), handle);
  }

  _last_image_count = current_count;
}

} // namespace editor
