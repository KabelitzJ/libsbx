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

float2 rotate(float2 position, float rotation) {
  float c = cos(rotation);
  float s = sin(rotation);

  return float2(
    position.x * c - position.y * s,
    position.x * s + position.y * c
  );
}

// [shader("vertex")]
// vs_out main(vs_in input) {
//   vs_out output;

//   uint particle_index = alive_list[input.instance_id];
//   particle particle = particles[particle_index];

//   float t = particle.age / particle.lifetime;

//   float4 color = lerp(particle.color, params.end_color, t);
//   float size = particle.size.x * lerp(1.0, params.end_size_scale, t);

//   int index = indices[input.vertex_id];
//   float3 local_position = vertices[index];

//   // Rotation
//   float c = cos(particle.rotation);
//   float s = sin(particle.rotation);

//   float2 rotated = rotate(local_position.xy, particle.rotation);

//   float3 world_position = particle.position + scene.camera_right * rotated.x * size + scene.camera_up * rotated.y * size;

//   output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));
//   output.uv = local_position.xy * 0.5 + 0.5;
//   output.color = color;

//   return output;
// }

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  // Keep resources "used" so the set/binding layout stays identical.
  uint pi = alive_list[0];
  float keep0 = particles[pi].age * 1e-6;
  float keep1 = scene.view[0][0] * 1e-6;
  float keep2 = params.end_size_scale * 1e-6;

  int idx = indices[input.vertex_id];
  float2 p = vertices[idx].xy * 0.25; // centered quad in NDC

  output.sv_position = float4(p.x + keep0 + keep1 + keep2, p.y, 0.0, 1.0);
  output.uv = float2(0.0, 0.0);
  output.color = float4(1.0, 0.0, 1.0, 1.0); // magenta, fully opaque

  return output;
}