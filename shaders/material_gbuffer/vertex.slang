#include <libsbx/common/vertex.slang>
#include <libsbx/common/convert_to.slang>
#include <libsbx/common/scene_data.slang>

#include <material_gbuffer/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
  uint base_vertex : SV_StartVertexLocation;
  uint base_instance : SV_StartInstanceLocation;
  uint view_id : SV_ViewID;
}; // struct vs_in

struct gbuffer_vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] nointerpolation uint instance_index : INSTANCE_INDEX;
#if defined(VERTEX_STREAM_COLOR)
  [[vk::location(5)]] float4 color : COLOR0;
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  [[vk::location(6)]] float4 custom0 : CUSTOM0;
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  [[vk::location(7)]] float4 custom1 : CUSTOM1;
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  [[vk::location(8)]] float4 custom2 : CUSTOM2;
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  [[vk::location(9)]] float4 custom3 : CUSTOM3;
#endif
  float4 sv_position : SV_Position;
}; // struct gbuffer_vs_out

struct shadow_vs_out {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
}; // struct shadow_vs_out

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

vertex_setup compute_vertex_setup(vs_in input) {
  vertex_setup setup;

  setup.global_vertex = input.vertex_id + input.base_vertex;
  setup.global_instance = input.instance_id + input.base_instance;

  vertex vertex = push.vertex_buffer[setup.global_vertex];
  instance_data instance_data = push.instance_data_buffer[setup.global_instance];
  transform_data transform_data = push.transform_data_buffer[instance_data.transform_index];
  setup.material = push.material_data_buffer[instance_data.material_index];

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float3 in_normal = float3(vertex.normal_x, vertex.normal_y, vertex.normal_z);
  float2 in_uv = float2(vertex.uv_x, vertex.uv_y);
  float4 in_tangent = float4(vertex.tangent_x, vertex.tangent_y, vertex.tangent_z, vertex.tangent_w);

  float3 world_position = mul(transform_data.model, float4(in_position, 1.0)).xyz;
  world_position = apply_sway(world_position, in_position.y, setup.material, scene.time, scene.wind_direction);

  setup.world_position = world_position;
  setup.world_normal = normalize(mul(transform_data.normal, float4(in_normal, 0.0)).xyz);
  setup.world_tangent = float4(normalize(mul(transform_data.normal, float4(in_tangent.xyz, 0.0)).xyz), in_tangent.w);
  setup.uv = in_uv;

  return setup;
}

[shader("vertex")]
gbuffer_vs_out gbuffer_main(vs_in input) {
  gbuffer_vs_out output;

  vertex_setup setup = compute_vertex_setup(input);

  output.position = setup.world_position;
  output.normal = setup.world_normal;
  output.tangent = setup.world_tangent;
  output.uv = setup.uv;
  output.instance_index = setup.global_instance;

#if defined(VERTEX_STREAM_COLOR)
  output.color = push.color_buffer[setup.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  output.custom0 = push.custom0_buffer[setup.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  output.custom1 = push.custom1_buffer[setup.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  output.custom2 = push.custom2_buffer[setup.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  output.custom3 = push.custom3_buffer[setup.global_vertex];
#endif

  output.sv_position = mul(scene.projection, mul(scene.view, float4(setup.world_position, 1.0)));

  return output;
}

[shader("vertex")]
shadow_vs_out shadow_main(vs_in input) {
  shadow_vs_out output;

  vertex_setup setup = compute_vertex_setup(input);

  output.uv = setup.uv;
  output.instance_index = setup.global_instance;
  output.sv_position = mul(scene.light_spaces[input.view_id], float4(setup.world_position, 1.0));

  return output;
}
