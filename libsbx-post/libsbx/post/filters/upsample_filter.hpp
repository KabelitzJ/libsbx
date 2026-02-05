// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_UPSAMPLE_FILTER_HPP_
#define LIBSBX_POST_FILTERS_UPSAMPLE_FILTER_HPP_

#include <filesystem>
#include <string>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

class upsample_filter final : public filter {

  using base = filter;

public:

  upsample_filter(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, std::string low_attachment, std::string base_attachment, std::float_t intensity = 1.0f)
  : base{attachments, path, base::default_pipeline_definition},
    _push_handler{base::pipeline()},
    _low_attachment{std::move(low_attachment)},
    _base_attachment{std::move(base_attachment)},
    _intensity{intensity} { }

  ~upsample_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    pipeline.bind(command_buffer);

    _push_handler.push("intensity", _intensity);

    descriptor_handler.push("low_image", graphics_module.attachment(_low_attachment));
    descriptor_handler.push("base_image", graphics_module.attachment(_base_attachment));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  graphics::push_handler _push_handler;
  std::string _low_attachment;
  std::string _base_attachment;
  std::float_t _intensity;

}; // class upsample_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_UPSAMPLE_FILTER_HPP_
