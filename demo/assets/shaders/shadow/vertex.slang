#include <libsbx/common/vertex.slang>
#include <libsbx/common/scene_data.slang>

#include <shadow/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
  uint base_vertex : SV_StartVertexLocation;
  uint base_instance : SV_StartInstanceLocation;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_index : INSTANCE_INDEX;
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

  instance_data instance_data = push.instance_data_buffer[global_instance];

  transform_data transform_data = push.transform_data_buffer[instance_data.transform_index];

  vertex vertex = push.vertex_buffer[global_vertex];

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float2 in_uv = float2(vertex.uv_x, vertex.uv_y);

  float3 world_position = mul(transform_data.model, float4(in_position, 1.0)).xyz;

  output.uv = in_uv;
  output.instance_index = global_instance;

  output.sv_position = mul(scene.light_spaces[push.cascade], float4(world_position, 1.0));

  return output;
}
