#include <libsbx/common/vertex.slang>
#include <libsbx/common/convert_to.slang>
#include <libsbx/common/scene_data.slang>

#include <common.slang>

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
ConstantBuffer<static_push_data> static_push;

[[vk::push_constant]] 
ConstantBuffer<skinned_push_data> skinned_push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float4x4 calculate_skinning_matrix(uint4 bone_indices, float4 bone_weights, float4x4* bone_matrices_buffer, uint bone_matrices_offset) {
  float4x4 result = float4x4(
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0
  );

  [unroll]
  for (int i = 0; i < 4; ++i) {
    result += bone_weights[i] * bone_matrices_buffer[bone_indices[i] + bone_matrices_offset];
  }

  return result;
}

[shader("vertex")]
vs_out static_main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  static_vertex vertex = static_push.vertex_buffer[global_vertex];

  static_instance_data instance_data = static_push.instance_data_buffer[global_instance];

  transform_data transform_data = static_push.transform_data_buffer[instance_data.transform_index];
  material_data material_data = static_push.material_data_buffer[instance_data.material_index];

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

[shader("vertex")]
vs_out skinned_main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  skinned_vertex vertex = skinned_push.vertex_buffer[global_vertex];

  skinned_instance_data instance_data = skinned_push.instance_data_buffer[global_instance];

  transform_data transform_data = skinned_push.transform_data_buffer[instance_data.transform_index];
  material_data material_data = skinned_push.material_data_buffer[instance_data.material_index];

  uint bone_matrices_offset = uint(instance_data.bone_matrix_offset);

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float3 in_normal = float3(vertex.normal_x, vertex.normal_y, vertex.normal_z);
  float4 in_tangent = float4(vertex.tangent_x, vertex.tangent_y, vertex.tangent_z, vertex.tangent_w);
  float2 in_uv = float2(vertex.uv_x, vertex.uv_y);
  uint4 in_bone_indices = uint4(vertex.bone_index_x, vertex.bone_index_y, vertex.bone_index_z, vertex.bone_index_w);
  float4 in_bone_weights = float4(vertex.bone_weight_x, vertex.bone_weight_y, vertex.bone_weight_z, vertex.bone_weight_w);

  float4x4 skinning_matrix = calculate_skinning_matrix(in_bone_indices, in_bone_weights, skinned_push.bone_matrices_buffer, bone_matrices_offset);

  float3 skinned_position = mul(skinning_matrix, float4(in_position, 1.0)).xyz;
  float3 skinned_normal = normalize(mul(skinning_matrix, float4(in_normal, 0.0)).xyz);
  float3 skinned_tangent = normalize(mul(skinning_matrix, float4(in_tangent.xyz, 0.0)).xyz);

  float3 world_position = mul(transform_data.model, float4(skinned_position, 1.0)).xyz;
  float3 normal = normalize(mul(transform_data.normal, float4(skinned_normal, 0.0)).xyz);
  float4 tangent = float4(normalize(mul(transform_data.normal, float4(skinned_tangent, 0.0)).xyz), in_tangent.w);

  output.position = world_position;
  output.normal = normal;
  output.tangent = tangent;
  output.uv = in_uv;
  output.instance_index = global_instance;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
