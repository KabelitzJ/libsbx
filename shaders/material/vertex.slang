#include <libsbx/common/vertex.slang>
#include <libsbx/common/convert_to.slang>
#include <libsbx/common/scene_data.slang>

#include <material/common.slang>

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

struct vertex_context {
  uint global_vertex;
  uint global_instance;
  vertex vertex;
  transform_data transform;
  material_data material;
}; // struct vertex_context

struct sway_state {
  float2 trunk;     // (speed, strength)
  float2 leaf;      // (speed, strength)
  float2 falloffs;  // (trunk_falloff, leaf_falloff)
}; // struct sway_state


float2 unpack_half2(float packed) {
  uint bits = asuint(packed);
  return float2(f16tof32(bits & 0xFFFF), f16tof32(bits >> 16));
}

sway_state load_sway(material_data material_data) {
  sway_state sway_state;

  sway_state.trunk = unpack_half2(material_data.sway_speed_strength);
  sway_state.leaf = unpack_half2(material_data.scrumble_speed_strength);
  sway_state.falloffs = unpack_half2(material_data.falloff_exponents);

  return sway_state;
}

float3 apply_sway(float3 world_position, float local_height, material_data material_data, float time, float2 wind_direction) {
  sway_state sway_state = load_sway(material_data);

  if (sway_state.trunk.y == 0.0 && sway_state.leaf.y == 0.0) {
    return world_position;
  }

  float height = saturate(local_height);

  float trunk_factor = pow(height, sway_state.falloffs.x);
  float trunk_wave = sin(time * sway_state.trunk.x) + 0.3 * sin(time * sway_state.trunk.x * 2.3 + 1.2);
  float2 trunk_offset = wind_direction * trunk_wave * sway_state.trunk.y * trunk_factor;

  float leaf_factor = pow(height, sway_state.falloffs.y);
  float spatial_hash = dot(world_position.xz, float2(12.9898, 78.233));
  float lx = sin(time * sway_state.leaf.x + spatial_hash) * sway_state.leaf.y * leaf_factor;
  float lz = sin(time * sway_state.leaf.x * 1.3 + spatial_hash * 0.7) * sway_state.leaf.y * leaf_factor;

  world_position.xz += trunk_offset + float2(lx, lz);

  return world_position;
}

vertex_context load_vertex_context(vs_in input) {
  vertex_context contex;

  contex.global_vertex = input.vertex_id + input.base_vertex;
  contex.global_instance = input.instance_id + input.base_instance;
  contex.vertex = push.vertex_buffer[contex.global_vertex];

  instance_data instance = push.instance_data_buffer[contex.global_instance];
  contex.transform = push.transform_data_buffer[instance.transform_index];
  contex.material = push.material_data_buffer[instance.material_index];

  return contex;
}

float3 compute_world_position(vertex_context contex) {
  float3 local = contex.vertex.position();
  float3 world = mul(contex.transform.model, float4(local, 1.0)).xyz;

  return apply_sway(world, local.y, contex.material, scene.time, scene.wind_direction);
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

  vertex_context context = load_vertex_context(input);

  float3 world_position = compute_world_position(context);

  output.position = world_position;
  output.normal = normalize(mul(context.transform.normal, float4(context.vertex.normal(), 0.0)).xyz);

  float3 world_tangent = normalize(mul(context.transform.normal, float4(context.vertex.tangent().xyz, 0.0)).xyz);
  output.tangent = float4(world_tangent, context.vertex.tangent().w);
  output.uv = context.vertex.uv();
  output.instance_index = context.global_instance;

#if defined(VERTEX_STREAM_COLOR)
  output.color = push.color_buffer[context.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  output.custom0 = push.custom0_buffer[context.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  output.custom1 = push.custom1_buffer[context.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  output.custom2 = push.custom2_buffer[context.global_vertex];
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  output.custom3 = push.custom3_buffer[context.global_vertex];
#endif

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}

[shader("vertex")]
shadow_vs_out shadow_main(vs_in input) {
  shadow_vs_out output;

  vertex_context context = load_vertex_context(input);
  float3 world_position = compute_world_position(context);

  output.uv = context.vertex.uv();
  output.instance_index = context.global_instance;
  output.sv_position = mul(scene.light_spaces[input.view_id], float4(world_position, 1.0));

  return output;
}
