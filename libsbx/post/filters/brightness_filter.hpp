// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_BRIGHTNESS_FILTER_HPP_
#define LIBSBX_POST_FILTERS_BRIGHTNESS_FILTER_HPP_

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

struct brightness_config {
  std::float_t threshold = 1.0f;
  std::float_t soft_knee = 0.5f;
}; // struct brightness_config

class brightness_filter final : public filter {

  using base = filter;

  inline static constexpr auto default_shader_path = std::string_view{"engine://shaders/brightness"};

public:

  brightness_filter(const std::vector<graphics::attachment_description>& attachments, const std::string& source_attachment, const brightness_config& config = brightness_config{}, const std::filesystem::path& path = default_shader_path)
  : base{attachments, path, base::default_pipeline_definition},
    _push_handler{base::pipeline()},
    _source_attachment{source_attachment},
    _config{config} { }

  ~brightness_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("brightness_filter::render");

    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    pipeline.bind(command_buffer);

    _push_handler.push("threshold", _config.threshold);
    _push_handler.push("soft_knee", _config.soft_knee);

    descriptor_handler.push("source_image", graphics_module.attachment(_source_attachment));

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  graphics::push_handler _push_handler;
  std::string _source_attachment;
  brightness_config _config;

}; // class brightness_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_BRIGHTNESS_FILTER_HPP_
