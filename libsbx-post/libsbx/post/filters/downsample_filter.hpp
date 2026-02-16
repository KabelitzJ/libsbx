// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_DOWNSAMPLE_FILTER_HPP_
#define LIBSBX_POST_FILTERS_DOWNSAMPLE_FILTER_HPP_

#include <libsbx/core/engine.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

class downsample_filter final : public filter {

  using base = filter;

public:

  downsample_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& source_attachment)
  : base{attachments, path, base::default_pipeline_definition},
    _source_attachment{source_attachment} { }

  ~downsample_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    pipeline.bind(command_buffer);

    descriptor_handler.push("source_image", graphics_module.attachment(_source_attachment));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  std::string _source_attachment;

}; // class downsample_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_DOWNSAMPLE_FILTER_HPP_
