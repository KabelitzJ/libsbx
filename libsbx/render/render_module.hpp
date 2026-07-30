// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_packet.hpp>
#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Owns the render thread and drives the frame loop. The main thread extracts the active
 * scene into a render_packet; the render thread consumes it and never touches the ECS.
 */
class render_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<graphics::graphics_module, assets::assets_module, scenes::scenes_module>;

  render_module();

  ~render_module();

  auto render() -> void;

  auto set_composite_pass(std::unique_ptr<render_pass> pass) -> void;

  auto set_packet_producer(std::function<std::unique_ptr<render_packet_extension>()> producer) -> void;

  [[nodiscard]] auto scene_image() const noexcept -> graphics::image_handle {
    return _scene_image;
  }

private:

  inline static constexpr auto light_capacity = std::uint32_t{256u};
  inline static constexpr auto transform_capacity = std::uint32_t{16384u};

  inline static constexpr auto prefiltered_levels = std::uint32_t{5u};

  struct baked_environment {
    std::uint32_t irradiance_index{0xFFFFFFFFu};
    std::uint32_t prefiltered_base{0u};
    std::uint32_t prefiltered_count{0u};
    graphics::image_handle irradiance_image{};
    std::vector<graphics::image_handle> prefiltered_images{};
  }; // struct baked_environment

  auto _start() -> void;

  auto _stop() -> void;

  auto _render_loop() -> void;

  auto _ensure_resources() -> void;

  auto _resize_targets(const math::vector2u extent) -> void;

  auto _prepare_frame(render_context& context) -> void;

  [[nodiscard]] auto _build_packet() -> render_packet;

  auto _consume_packet(const render_packet& packet) -> void;

  auto _ensure_ibl_pipelines() -> void;

  auto _ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void;

  auto _bake_environment(graphics::command_buffer& command_buffer, const math::uuid& id, std::uint32_t radiance_index) -> void;

  auto _bake_fullscreen(graphics::command_buffer& command_buffer, graphics::image& target, const math::vector2u& extent, graphics::graphics_pipeline& pipeline, std::span<const std::byte> push_data) -> void;

  std::thread _thread{};

  std::mutex _mutex{};
  std::condition_variable _has_produced{};
  std::condition_variable _has_consumed{};

  render_packet _packet{};
  bool _has_packet{false};
  bool _is_running{false};

  std::uint32_t _sampler_index{0u};

  graphics::image_handle _depth_image{};
  graphics::image_handle _color_image{};
  graphics::image_handle _color_msaa_image{};
  std::uint32_t _color_index{0u};
  math::vector2u _target_extent{};

  std::function<std::unique_ptr<render_packet_extension>()> _packet_producer{};

  std::vector<std::unique_ptr<render_pass>> _passes{};
  std::unique_ptr<render_pass> _composite_pass{};

  graphics::image_handle _scene_image{};
  std::uint32_t _scene_index{0u};

  graphics::buffer_handle _frame_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _frame_addresses{};

  graphics::buffer_handle _light_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _light_addresses{};

  graphics::buffer_handle _transform_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _transform_addresses{};

  memory::observer_ptr<graphics::graphics_pipeline> _brdf_lut_pipeline{nullptr};
  memory::observer_ptr<graphics::graphics_pipeline> _irradiance_pipeline{nullptr};
  memory::observer_ptr<graphics::graphics_pipeline> _prefilter_pipeline{nullptr};

  graphics::image_handle _brdf_lut_image{};
  std::uint32_t _brdf_lut_index{0xFFFFFFFFu};

  std::unordered_map<math::uuid, baked_environment> _baked_environments{};

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
