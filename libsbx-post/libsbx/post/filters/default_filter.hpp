// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_DEFAULT_FILTER_HPP_
#define LIBSBX_POST_FILTERS_DEFAULT_FILTER_HPP_

#include <string>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

template<std::size_t Id>
class default_filter final : public filter {

  using base_type = filter;

public:

  default_filter(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name)
  : base_type{attachments, path},
    _attachment_name{attachment_name} { }

  ~default_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base_type::pipeline();
    auto& descriptor_handler = base_type::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    pipeline.bind(command_buffer);

    descriptor_handler.push("image", graphics_module.attachment(_attachment_name));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  std::string _attachment_name;

}; // class default_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_DEFAULT_FILTER_HPP_
