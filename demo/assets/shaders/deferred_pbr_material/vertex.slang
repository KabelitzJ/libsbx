#include <libsbx/common/vertex.slang>
#include <libsbx/common/convert_to.slang>
#include <libsbx/common/scene_data.slang>

#include <deferred_pbr_material/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
  uint base_vertex : SV_StartVertexLocation;
  uint base_instance : SV_StartInstanceLocation;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
}; // struct vs_out

[[vk::push_constant]] 
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float2 unpack_half2(float packed) {
  uint bits = asuint(packed);
  return float2(f16tof32(bits & 0xFFFF), f16tof32(bits >> 16));
}

float3 apply_sway(float3 world_position, float local_height, material_data material, float time, float2 wind_direction) {
  float2 sway = unpack_half2(material.sway_speed_strength);
  float2 scrumble = unpack_half2(material.scrumble_speed_strength);
  float2 falloffs = unpack_half2(material.falloff_exponents);

  float sway_speed = sway.x;
  float sway_strength = sway.y;
  float scrumble_speed = scrumble.x;
  float scrumble_strength = scrumble.y;
  float sway_falloff = falloffs.x;
  float scrumble_falloff = falloffs.y;

  if (sway_strength == 0.0 && scrumble_strength == 0.0) {
    return world_position;
  }

  float height_factor = saturate(local_height);

  // Trunk sway
  float trunk_factor = pow(height_factor, sway_falloff);
  float trunk_wave = sin(time * sway_speed) + 0.3 * sin(time * sway_speed * 2.3 + 1.2);
  float2 trunk_offset = normalize(wind_direction) * trunk_wave * sway_strength * trunk_factor;

  // Leaf scrumble
  float scrumble_factor = pow(height_factor, scrumble_falloff);
  float spatial_hash = dot(world_position.xz, float2(12.9898, 78.233));
  float sx = sin(time * scrumble_speed + spatial_hash) * scrumble_strength * scrumble_factor;
  float sz = sin(time * scrumble_speed * 1.3 + spatial_hash * 0.7) * scrumble_strength * scrumble_factor;

  world_position.x += trunk_offset.x + sx;
  world_position.z += trunk_offset.y + sz;

  return world_position;
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  vertex vertex = push.vertex_buffer[global_vertex];

  instance_data instance_data = push.instance_data_buffer[global_instance];

  transform_data transform_data = push.transform_data_buffer[instance_data.transform_index];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float3 in_normal = float3(vertex.normal_x, vertex.normal_y, vertex.normal_z);
  float2 in_uv = float2(vertex.uv_x, vertex.uv_y);
  float4 in_tangent = float4(vertex.tangent_x, vertex.tangent_y, vertex.tangent_z, vertex.tangent_w);

  float3 world_position = mul(transform_data.model, float4(in_position, 1.0)).xyz;

  world_position = apply_sway(world_position, in_position.y, material_data, scene.time, scene.wind_direction);

  output.position = world_position;
  output.normal = normalize(mul(transform_data.normal, float4(in_normal, 0.0)).xyz);
  output.tangent = float4(normalize(mul(transform_data.normal, float4(in_tangent.xyz, 0.0)).xyz), in_tangent.w);
  output.uv = in_uv;
  output.instance_index = global_instance;

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
