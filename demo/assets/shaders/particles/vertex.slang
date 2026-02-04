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

static const float2 vertices[4] = {
  float2(-1.0, -1.0),
  float2( 1.0, -1.0),
  float2( 1.0,  1.0),
  float2(-1.0,  1.0)
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

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint particle_index = alive_list[input.instance_id];
  particle particle = particles[particle_index];

  float t = particle.age / particle.lifetime;

  float4 color = lerp(particle.color, params.end_color, t);
  float size = particle.size.x * lerp(1.0, params.end_size_scale, t);

  int index = indices[input.vertex_id];
  float2 local_position = vertices[index];

  float3x3 inverse_view = transpose((float3x3)scene.view);

  float3 camera_right = normalize(mul(inverse_view, float3(1.0, 0.0, 0.0)));
  float3 camera_up = normalize(mul(inverse_view, float3(0.0, 1.0, 0.0)));

  // Rotation
  float2 rotated = rotate(local_position, particle.rotation);

  // Billboard
  float3 offset = camera_right * rotated.x * size + camera_up * rotated.y * size;
  float3 world_position = particle.position + offset;

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));
  output.uv = local_position.xy * 0.5 + 0.5;
  output.color = color;

  return output;
}
