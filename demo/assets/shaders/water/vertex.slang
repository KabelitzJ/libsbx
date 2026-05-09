#include <libsbx/common/scene_data.slang>

#include <water/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  float4 sv_position : SV_Position;
}; // struct vs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

void wave_height_and_gradient(float2 pos, out float h, out float dh_dx, out float dh_dz) {
  float a = push.wave_amplitude;
  float f = push.wave_frequency;
  float s = push.wave_speed;
  float t = scene.time;

  float p1 = pos.x * f + t * s;
  float p2 = pos.y * f * 1.13 + t * s * 1.27 + 1.0;
  float p3 = (pos.x * 0.7 + pos.y * 0.7) * f * 2.7 + t * s * 1.5;

  float a1 = a;
  float a2 = a * 0.7;
  float a3 = a * 0.3;

  h = sin(p1) * a1 + sin(p2) * a2 + sin(p3) * a3;

  dh_dx = cos(p1) * f * a1 + cos(p3) * 0.7 * f * 2.7 * a3;
  dh_dz = cos(p2) * f * 1.13 * a2 + cos(p3) * 0.7 * f * 2.7 * a3;
}

[shader("vertex")]
vs_out main(vs_in input) {
  uint verts_per_side = push.grid_size + 1;
  uint row = input.vertex_id / verts_per_side;
  uint col = input.vertex_id % verts_per_side;

  float u = float(col) / float(push.grid_size);
  float v = float(row) / float(push.grid_size);

  float world_x = lerp(push.bounds_min_x, push.bounds_max_x, u);
  float world_z = lerp(push.bounds_min_z, push.bounds_max_z, v);

  float h, dh_dx, dh_dz;
  wave_height_and_gradient(float2(world_x, world_z), h, dh_dx, dh_dz);

  float3 world_position = float3(world_x, push.water_y + h, world_z);
  float3 normal = normalize(float3(-dh_dx, 1.0, -dh_dz));

  vs_out output;

  output.position = world_position;
  output.normal = normal;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
