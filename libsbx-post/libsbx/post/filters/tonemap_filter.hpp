// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_TONEMAP_FILTER_HPP_
#define LIBSBX_POST_FILTERS_TONEMAP_FILTER_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/core/settings.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

struct tonemap_config {
  std::float_t exposure = 0.0f;
  std::float_t bloom_mix = 0.1f;
  std::float_t saturation = 1.0f;
  std::float_t contrast = 1.0f;
  std::float_t temperature = 0.0f;
  std::float_t tint = 0.0f;
}; // struct tonemap_config

class tonemap_filter final : public filter {

  using base = filter;

  inline static constexpr auto exposure_key = core::prototype::setting_key<std::float_t>{"render.exposure", core::prototype::setting_range<std::float_t>{-5.0f, 5.0f}};
  inline static constexpr auto bloom_mix_key = core::prototype::setting_key<std::float_t>{"render.bloom_mix", core::prototype::setting_range<std::float_t>{0.0f, 0.4f}};
  inline static constexpr auto saturation_key = core::prototype::setting_key<std::float_t>{"render.saturation", core::prototype::setting_range<std::float_t>{0.0f, 1.0f}};
  inline static constexpr auto contrast_key = core::prototype::setting_key<std::float_t>{"render.contrast", core::prototype::setting_range<std::float_t>{0.0f, 1.0f}};
  inline static constexpr auto temperature_key = core::prototype::setting_key<std::float_t>{"render.temperature", core::prototype::setting_range<std::float_t>{-1.0f, 1.0f}};
  inline static constexpr auto tint_key = core::prototype::setting_key<std::float_t>{"render.tint", core::prototype::setting_range<std::float_t>{-1.0f, 1.0f}};

public:

  tonemap_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, std::vector<std::pair<std::string, std::string>>&& attachment_names, const tonemap_config& tonemap_config = post::tonemap_config{})
  : base{attachments, path, base::default_pipeline_definition},
    _push_handler{base::pipeline()},
    _attachment_names{std::move(attachment_names)},
    _tonemap_config{tonemap_config} {
    core::prototype::settings::set(exposure_key, _tonemap_config.exposure);
    core::prototype::settings::set(bloom_mix_key, _tonemap_config.bloom_mix);
    core::prototype::settings::set(saturation_key, _tonemap_config.saturation);
    core::prototype::settings::set(contrast_key, _tonemap_config.contrast);
    core::prototype::settings::set(temperature_key, _tonemap_config.temperature);
    core::prototype::settings::set(tint_key, _tonemap_config.tint);
  }

  ~tonemap_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
    pipeline.bind(command_buffer);

    _push_handler.push("exposure", _tonemap_config.exposure);
    _push_handler.push("bloom_mix", _tonemap_config.bloom_mix);
    _push_handler.push("saturation", _tonemap_config.saturation);
    _push_handler.push("contrast", _tonemap_config.contrast);
    _push_handler.push("temperature", _tonemap_config.temperature);
    _push_handler.push("tint", _tonemap_config.tint);

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

private:

  graphics::push_handler _push_handler;
  std::vector<std::pair<std::string, std::string>> _attachment_names;
  tonemap_config _tonemap_config;

}; // class tonemap_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_TONEMAP_FILTER_HPP_
