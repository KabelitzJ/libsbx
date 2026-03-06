#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct push_data {
  float* height_data_buffer;
  uint verts_w;
  uint verts_h;
  float inv_verts_w;
  float cell_size;
  float offset_x;
  float offset_z;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint y = uint(float(input.vertex_id) * push.inv_verts_w);
  uint x = input.vertex_id - y * push.verts_w;

  float h = push.height_data_buffer[input.vertex_id];

  float3 world_pos = float3(
    float(x) * push.cell_size - push.offset_x,
    h,
    float(y) * push.cell_size - push.offset_z
  );

  // Central difference normals from neighbors
  uint x_neg = max(x, 1u) - 1u;
  uint x_pos = min(x + 1u, push.verts_w - 1u);
  uint y_neg = max(y, 1u) - 1u;
  uint y_pos = min(y + 1u, push.verts_h - 1u);

  float h_left  = push.height_data_buffer[y * push.verts_w + x_neg];
  float h_right = push.height_data_buffer[y * push.verts_w + x_pos];
  float h_down  = push.height_data_buffer[y_neg * push.verts_w + x];
  float h_up    = push.height_data_buffer[y_pos * push.verts_w + x];

  float step = 2.0 * push.cell_size;

  float3 normal = normalize(float3(
    (h_left - h_right) / step,
    1.0,
    (h_down - h_up) / step
  ));

  output.position = world_pos;
  output.normal = normal;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_pos, 1.0)));

  return output;
}
