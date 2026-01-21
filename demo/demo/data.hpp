#ifndef DEMO_DATA_HPP_
#define DEMO_DATA_HPP_

namespace demo {

struct grid_data {
  bool is_painted = false;
}; // struct grid_data

struct dual_quad_tile_data {
  sbx::math::uuid mesh_id{};
  sbx::math::color color = sbx::math::color::white();
  std::float_t height = 3.0f;
  std::uint32_t rotation_steps = 0u;
  std::uint8_t last_mask = 0u;
  bool is_visible = false;
}; // struct dual_quad_tile_data

} // namespace demo

#endif // DEMO_DATA_HPP_
