// SPDX-License-Identifier: MIT
#ifndef EDITOR_FILTERS_SELECTION_FILTER_HPP_
#define EDITOR_FILTERS_SELECTION_FILTER_HPP_

#include <libsbx/post/filter.hpp>

namespace editor {

class selection_filter final : public sbx::post::filter {

  using base = sbx::post::filter;

public:

  selection_filter(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path, std::vector<std::pair<std::string, std::string>>&& attachment_names);

  ~selection_filter() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

private:

  sbx::graphics::push_handler _push_handler;
  std::vector<std::pair<std::string, std::string>> _attachment_names;

}; // class selection_filter

} // namespace editor

#endif // EDITOR_FILTERS_SELECTION_FILTER_HPP_
