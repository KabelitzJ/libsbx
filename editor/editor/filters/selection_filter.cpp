// SPDX-License-Identifier: MIT
#include <editor/filters/selection_filter.hpp>

#include <libsbx/core/profiler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <editor/editor_module.hpp>

namespace editor {

selection_filter::selection_filter(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path, std::vector<std::pair<std::string, std::string>>&& attachment_names)
: base{attachments, path},
  _push_handler{base::pipeline()},
  _attachment_names{std::move(attachment_names)} { }

auto selection_filter::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("selection_filter::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  auto& pipeline = base::pipeline();
  auto& descriptor_handler = base::descriptor_handler();

  pipeline.bind(command_buffer);

  const auto selected = editor_module.selected_node();
  const auto selected_id = static_cast<std::uint32_t>(selected);

  _push_handler.push("color", sbx::math::color{1.0f, 0.86f, 0.49f, 1.0f});
  _push_handler.push("thickness", 2.0f);
  _push_handler.push("selected_id", selected_id);

  for (const auto& [name, attachment] : _attachment_names) {
    descriptor_handler.push(name, graphics_module.attachment(attachment));
  }

  if (!descriptor_handler.update(pipeline)) {
    return;
  }

  descriptor_handler.bind_descriptors(command_buffer);
  _push_handler.bind(command_buffer);

  command_buffer.draw(3, 1, 0, 0);
}

} // namespace editor
