// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FXAA_FILTER_HPP_
#define LIBSBX_POST_FXAA_FILTER_HPP_

#include <libsbx/post/filter.hpp>

namespace sbx::post {

class fxaa_filter final : public filter {

  using base_type = filter;

public:

  fxaa_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& in_image)
  : base_type{attachments, path},
    _in_image{in_image} { }

  ~fxaa_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    EASY_BLOCK("fxaa_filter::render");
    SBX_PROFILE_SCOPE("fxaa_filter::render");

    auto& pipeline = base_type::pipeline();
    auto& descriptor_handler = base_type::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    pipeline.bind(command_buffer);

    descriptor_handler.push("image", graphics_module.attachment(_in_image));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  std::string _in_image;
  std::string _out_image;

}; // class blur_filter

} // namespace sbx::post 

#endif // LIBSBX_POST_FXAA_FILTER_HPP_
