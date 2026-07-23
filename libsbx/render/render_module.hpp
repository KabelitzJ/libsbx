// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_packet.hpp>

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

private:

  auto _start() -> void;

  auto _stop() -> void;

  auto _render_loop() -> void;

  [[nodiscard]] auto _build_packet() -> render_packet;

  auto _consume_packet(const render_packet& packet) -> void;

  std::thread _thread{};

  std::mutex _mutex{};
  std::condition_variable _has_produced{};
  std::condition_variable _has_consumed{};

  render_packet _packet{};
  bool _has_packet{false};
  bool _is_running{false};
  bool _is_started{false};

  std::uint32_t _sampler_index{0u};

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

  graphics::image_handle _depth_image{};
  std::uint32_t _depth_width{0u};
  std::uint32_t _depth_height{0u};

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
