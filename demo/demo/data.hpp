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
}; // struct terrain_tag

} // namespace demo

#endif // DEMO_DATA_HPP_
