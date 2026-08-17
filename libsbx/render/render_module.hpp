// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/delegate.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_packet.hpp>
#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_thread.hpp>

namespace sbx::render {

/**
 * @brief Drives the frame loop via a render_thread (see render_thread.hpp), whose threading policy
 * comes from core::engine::config(). The calling (main) thread always extracts the active scene
 * into a render_packet; whichever thread ends up consuming it (itself, or a dedicated render
 * thread, depending on policy) never touches the ECS.
 */
class render_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<graphics::graphics_module, assets::assets_module, scenes::scenes_module>;

  render_module();

  ~render_module();

  auto render() -> void;

  auto set_composite_pass(std::unique_ptr<render_pass> pass) -> void;

  auto set_pre_render_callback(core::delegate<void()> callback) -> void;

  [[nodiscard]] auto scene_image() const noexcept -> graphics::image_handle {
    return _scene_image;
  }

private:

  inline static constexpr auto light_capacity = std::uint32_t{256u};
  inline static constexpr auto transform_capacity = std::uint32_t{16384u};

  auto _ensure_resources() -> void;

  auto _resize_targets(const math::vector2u extent) -> void;

  auto _prepare_frame(render_context& context) -> void;

  [[nodiscard]] auto _build_packet() -> render_packet;

  auto _consume_packet(const render_packet& packet) -> void;

  std::unique_ptr<render_thread> _render_thread{};
  render_packet _work_packet{};

  std::uint32_t _sampler_index{0u};

  graphics::image_handle _depth_image{};
  graphics::image_handle _color_image{};
  graphics::image_handle _color_msaa_image{};
  std::uint32_t _color_index{0u};
  math::vector2u _target_extent{};

  core::delegate<void()> _pre_render_callback{};

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

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
