// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_packet.hpp>

namespace sbx::render {

/**
 * @brief Owns the render stages and drives the frame loop.
 */
class render_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<graphics::graphics_module, assets::assets_module, scenes::scenes_module>;

  render_module();

  ~render_module();

  auto render() -> void;

  /**
   * @brief Main thread. The texture the render thread samples once it is resident. The app holds the
   * asset; the render thread only reads its bindless index.
   */
  auto set_display_texture(assets::texture_handle texture) -> void {
    _display_texture = std::move(texture);
  }

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

  assets::texture_handle _display_texture{};
  std::uint32_t _sampler_index{0u};

  std::unique_ptr<graphics::shader> _shader{};
  std::unique_ptr<graphics::graphics_pipeline> _pipeline{};

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
