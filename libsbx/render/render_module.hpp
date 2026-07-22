// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_MODULE_HPP_
#define LIBSBX_RENDER_RENDER_MODULE_HPP_

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

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

  using dependencies = core::dependency_list<assets::assets_module, graphics::graphics_module, scenes::scenes_module>;

  render_module();

  ~render_module();

  auto render() -> void;

  auto upload_texture(std::vector<std::byte> pixels, std::uint32_t width, std::uint32_t height, graphics::format format) -> void;

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

  struct pending_texture {
    std::vector<std::byte> pixels;
    std::uint32_t width;
    std::uint32_t height;
    graphics::format format;
  }; // struct pending_texture

  // Main thread posts here; the render thread drains it in _consume.
  std::mutex _texture_mutex{};
  std::optional<pending_texture> _pending_texture{};

  graphics::image_handle _texture{};
  std::uint32_t _texture_index{0u};
  std::uint32_t _sampler_index{0u};
  std::uint64_t _texture_frame{0u};
  bool _has_texture{false};

  std::unique_ptr<graphics::shader> _shader{};
  std::unique_ptr<graphics::graphics_pipeline> _pipeline{};

}; // class render_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_MODULE_HPP_
