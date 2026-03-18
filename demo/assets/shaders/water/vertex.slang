#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 world_xz : WORLD_XZ;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct push_data {
  float* height_data_buffer;
  float chunk_world_x;
  float chunk_world_z;
  float chunk_size;
  float water_level;
  float terrain_offset_x;
  float terrain_offset_z;
  float base_cell_size;
  uint vertices_x;
  uint vertices_z;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

static const float2 positions[6] = {
  float2(0.0, 0.0),
  float2(1.0, 0.0),
  float2(0.0, 1.0),
  float2(0.0, 1.0),
  float2(1.0, 0.0),
  float2(1.0, 1.0)
};

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  float2 local_position = positions[input.vertex_id];

  float world_x = push.chunk_world_x + local_position.x * push.chunk_size;
  float world_z = push.chunk_world_z + local_position.y * push.chunk_size;

  float3 world_position = float3(world_x, push.water_level, world_z);

  output.position = world_position;
  output.normal = float3(0.0, 1.0, 0.0);
  output.world_xz = float2(world_x, world_z);
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
