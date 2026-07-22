// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

#include <libsbx/math/color.hpp>

namespace sbx::render {

/**
 * @brief An immutable snapshot of everything the render side needs to draw one 
 * frame, produced by the main thread's extract phase and consumed by the render side.
 */
struct render_packet {
  math::color clear_color{0.7f, 0.2f, 0.2f, 1.0f};
}; // struct render_packet

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PACKET_HPP_
