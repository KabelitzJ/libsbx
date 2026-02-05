// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_BLOOM_FILTER_HPP_
#define LIBSBX_POST_FILTERS_BLOOM_FILTER_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {


class bloom_filter final : public filter {

  using base = filter;

public:

  bloom_filter(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& attachment_name, std::float_t threshold, std::float_t intensity)
  : base{attachments, path, base::default_pipeline_definition},
    _push_handler{base::pipeline()},
    _attachment_name{attachment_name},
    _threshold{threshold},
    _intensity{intensity} { }

  ~bloom_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
    pipeline.bind(command_buffer);

    // _push_handler.push("threshold", 1.0f);
    // _push_handler.push("intensity", 1.0f);
    // _push_handler.push("radius", 10.0f);

    descriptor_handler.push("image", graphics_module.attachment(_attachment_name));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    // _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  graphics::push_handler _push_handler;

  std::string _attachment_name;
  math::vector2 _threshold;
  math::vector2 _intensity;

}; // class bloom_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_BLOOM_FILTER_HPP_
