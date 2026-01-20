#ifndef DEMO_DATA_HPP_
#define DEMO_DATA_HPP_

namespace demo {

struct grid_data {
  bool is_painted = false;
  std::uint8_t last_mask = 0u;

  sbx::scenes::node node{sbx::scenes::node::null};

  // Optional: keep stable visuals per cell
  std::float_t height = 3.0f;
  sbx::math::color color = sbx::math::color::white();
}; // struct grid_data

struct terrain_tag {
  sbx::math::color color;
  sbx::math::uuid mesh_id;
  std::uint32_t grid_cell;
  std::float_t height;
  std::uint32_t rotation_steps = 0u;
  bool is_visible = true;
}; // struct terrain_tag

struct dual_quad_tile_data {
  sbx::scenes::node node = sbx::scenes::node::null;
  std::uint8_t last_mask = 0u;
  std::float_t height = 3.0f;
  sbx::math::color color = sbx::math::color::white();
  std::uint32_t rotation_steps = 0u;
}; // struct dual_quad_tile_data

} // namespace demo

#endif // DEMO_DATA_HPP_
