// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_SSAO_FILTER_HPP_
#define LIBSBX_POST_FILTERS_SSAO_FILTER_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/random.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {


class ssao_filter final : public filter {

  using base = filter;

public:

  inline static constexpr auto kernel_size = std::uint32_t{32u};
  inline static constexpr auto kernel_radius = std::float_t{0.5f};
  inline static constexpr auto noise_dimension = std::uint32_t{8u};

  ssao_filter(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, std::vector<std::pair<std::string, std::string>>&& attachment_names)
  : base{attachments, path, base::default_pipeline_definition},
    _push_handler{base::pipeline()},
    _attachment_names{attachment_names} {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto kernel = std::vector<math::vector3>{};
    kernel.resize(kernel_size);

    for (auto i = 0u; i < kernel.size(); ++i) {
      auto sample = math::vector3{
        math::random::next<std::float_t>(0.0f, 1.0f) * 2.0f - 1.0f, 
        math::random::next<std::float_t>(0.0f, 1.0f) * 2.0f - 1.0f, 
        math::random::next<std::float_t>(0.0f, 1.0f)
      };
      sample = math::vector3::normalized(sample);
      sample *= math::random::next<std::float_t>(0.0f, 1.0f);

      auto t = static_cast<std::float_t>(i) / static_cast<std::float_t>(kernel.size());
      auto scale = 0.1f + 0.9f * (t * t);

      kernel[i] = sample * scale;
    }

    _kernel = graphics_module.add_resource<graphics::storage_buffer>(kernel.size() * sizeof(math::vector3), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, kernel.data());

    auto noise = std::vector<math::vector4>{};
    noise.resize(noise_dimension * noise_dimension);

    for (auto i = 0u; i < noise.size(); ++i) {
      auto n = math::vector4{
        math::random::next<std::float_t>(0.0f, 1.0f) * 2.0f - 1.0f, 
        math::random::next<std::float_t>(0.0f, 1.0f) * 2.0f - 1.0f, 
        0.0f, 
        0.0f
      };

      noise[i] = math::vector4::normalized(n);
    }

    _noise = graphics_module.add_resource<graphics::image2d>(math::vector2u{noise_dimension, noise_dimension}, graphics::format::r8g8b8a8_unorm, graphics::filter::nearest, reinterpret_cast<const std::uint8_t*>(noise.data()));
  }

  ~ssao_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    EASY_BLOCK("ssao_filter::render");
    SBX_PROFILE_SCOPE("ssao_filter::render");

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();

    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();
    
    pipeline.bind(command_buffer);

    auto& kernel = graphics_module.get_resource<graphics::storage_buffer>(_kernel);
    auto& noise = graphics_module.get_resource<graphics::image2d>(_noise);

    _push_handler.push("kernel_size", kernel_size);
    _push_handler.push("kernel_radius", kernel_radius);
    _push_handler.push("kernel_buffer", kernel.address());
    _push_handler.push("bias", 0.01f);
    
    for (const auto& [name, attachment] : _attachment_names) {
      descriptor_handler.push(name, graphics_module.attachment(attachment));
    }

    descriptor_handler.push("noise_image", noise);
    descriptor_handler.push("scene", environment.uniform_handler());

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3, 1, 0, 0);
  }

private:

  inline static constexpr auto _lerp(std::float_t a, std::float_t b, std::float_t f) -> std::float_t {
		return a + f * (b - a);
	}

  graphics::push_handler _push_handler;

  graphics::storage_buffer_handle _kernel;
  graphics::image2d_handle _noise;

  std::vector<std::pair<std::string, std::string>> _attachment_names;

}; // class ssao_filter

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_SSAO_FILTER_HPP_
