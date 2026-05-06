// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
#define EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <libsbx/graphics/images/image2d.hpp>

#include <editor/panels/texture_cache.hpp>

namespace editor {

class asset_browser_panel {

  inline static constexpr auto payload_image_handle = "sbx_image2d_handle";

  enum class filter_kind : std::uint8_t {
    all,
    images,
    meshes
  }; // enum class filter_kind

  enum class folder_action : std::uint8_t {
    none,
    create,
    rename,
    remove
  }; // enum class folder_action

public:

  explicit asset_browser_panel(texture_cache& cache)
  : _texture_cache{cache} { }

  ~asset_browser_panel() = default;

  asset_browser_panel(const asset_browser_panel&) = delete;

  auto operator=(const asset_browser_panel&) -> asset_browser_panel& = delete;

  auto draw() -> void;

  static constexpr auto image_handle_payload() -> const char* {
    return payload_image_handle;
  }

private:

  auto _draw_tree(const std::filesystem::path& directory) -> void;

  auto _draw_breadcrumb(const std::filesystem::path& root) -> void;

  auto _draw_grid() -> void;

  auto _draw_file_card(const std::filesystem::directory_entry& entry) -> void;

  auto _refresh_image_cache() -> void;

  auto _draw_folder_context_menu(const std::filesystem::path& path) -> void;

  auto _draw_folder_dialogs() -> void;

  auto _begin_create(const std::filesystem::path& parent) -> void;

  auto _begin_rename(const std::filesystem::path& target) -> void;

  auto _begin_remove(const std::filesystem::path& target) -> void;

  auto _commit_create() -> bool;

  auto _commit_rename() -> bool;

  auto _commit_remove() -> bool;

  auto _on_directory_structure_changed() -> void;

  static auto _is_valid_folder_name(std::string_view name) -> bool;

  static auto _is_under(const std::filesystem::path& path, const std::filesystem::path& prefix) -> bool;

  static auto _remap_under(const std::filesystem::path& path, const std::filesystem::path& old_prefix, const std::filesystem::path& new_prefix) -> std::filesystem::path;

  texture_cache& _texture_cache;

  std::filesystem::path _root;
  std::filesystem::path _selected_directory;

  std::unordered_set<std::string> _expanded_directories;

  std::unordered_map<std::string, sbx::graphics::image2d_handle> _path_to_image;
  std::size_t _last_image_count{0};

  std::string _search_buffer;
  std::float_t _item_size{96.0f};
  std::float_t _tree_width{0.0f};

  filter_kind _active_filter{filter_kind::all};

  folder_action _pending_action{folder_action::none};
  std::filesystem::path _action_target;
  std::string _action_name_buffer;
  std::string _action_error;
  bool _action_focus_input{false};
  bool _action_request_open{false};

}; // class asset_browser_panel

} // namespace editor

#endif // EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
