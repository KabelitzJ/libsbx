// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::render {

struct render_item {
  math::matrix4x4 model{math::matrix4x4::identity};
  assets::mesh_handle mesh{};
  std::vector<assets::material_handle> materials{};
}; // struct render_item

struct camera_data {
  math::matrix4x4 view{math::matrix4x4::identity};
  math::vector3f position{0.0f, 0.0f, 0.0f};
  float fov_degrees{60.0f};
  float near_plane{0.1f};
  float far_plane{1000.0f};
  bool active{false};
}; // struct camera_data

struct render_packet {
  math::color clear_color{0.05f, 0.05f, 0.08f, 1.0f};
  camera_data camera{};
  std::vector<render_item> items{};
}; // struct render_packet

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PACKET_HPP_
