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

  output.position = world_position;
  output.normal = normalize(mul(transform_data.normal, float4(in_normal, 0.0)).xyz);
  output.tangent = float4(normalize(mul(transform_data.normal, float4(in_tangent.xyz, 0.0)).xyz), in_tangent.w);
  output.uv = in_uv;
  output.instance_index = global_instance;

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
