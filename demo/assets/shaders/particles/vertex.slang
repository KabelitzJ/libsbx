#include <libsbx/common/scene_data.slang>

#include <particles/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct vs_out

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
StructuredBuffer<particle> particles;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> alive_list;

[[vk::push_constant]]
ConstantBuffer<render_params> params;

static const float3 vertices[4] = {
  float3(-1.0, -1.0, 0.0),
  float3( 1.0, -1.0, 0.0),
  float3( 1.0,  1.0, 0.0),
  float3(-1.0,  1.0, 0.0)
};

static const int indices[6] = { 0, 1, 2, 2, 3, 0 };

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint particle_index = alive_list[input.instance_id];
  particle p = particles[particle_index];

  float t = p.age / p.lifetime;

  float4 color = lerp(p.color, params.end_color, t);
  float size = p.size.x * lerp(1.0, params.end_size_scale, t);

  int index = indices[input.vertex_id];
  float3 local_position = vertices[index];

  // Rotation
  float c = cos(p.rotation);
  float s = sin(p.rotation);
  float2 rotated = float2(
    local_position.x * c - local_position.y * s,
    local_position.x * s + local_position.y * c
  );

  // Billboard
  float3 world_position = p.position 
    + scene.camera_right * rotated.x * size 
    + scene.camera_up * rotated.y * size;

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));
  output.uv = local_position.xy * 0.5 + 0.5;
  output.color = color;

  return output;
}
