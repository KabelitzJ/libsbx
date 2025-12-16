struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

struct static_vertex {
  float position_x;
  float position_y;
  float position_z;

  float normal_x;
  float normal_y;
  float normal_z;

  float uv_x;
  float uv_y;

  float tangent_x;
  float tangent_y;
  float tangent_z;
  float tangent_w; // sign of the tangent
}; // struct static_vertex

struct skinned_vertex {
  float position_x;
	float position_y;
	float position_z;

	float normal_x;
	float normal_y;
	float normal_z;

	float uv_x;
	float uv_y;

  float tangent_x;
  float tangent_y;
  float tangent_z;
  float tangent_w; // sign of the tangent

  uint bone_index_x;
  uint bone_index_y;
  uint bone_index_z;
  uint bone_index_w;

  float bone_weight_x;
  float bone_weight_y;
  float bone_weight_z;
  float bone_weight_w;
}; // struct skinned_vertex

struct push_data { 
  skinned_vertex* skinned_vertices_buffer; 
  static_vertex* static_vertices_buffer; 
  float4x4* bone_matrices_buffer; 
  uint vertex_count; 
  uint bone_matrices_offset; 
  uint instance_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

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

[shader("compute")]
[numthreads(64, 1, 1)]
void main(cs_in input) {
  uint id = input.id.x;

  if (id >= push.vertex_count) {
    return;
  }

  skinned_vertex input = push.skinned_vertices_buffer[id];

  float3 in_position = float3(input.position_x, input.position_y, input.position_z);
  float3 in_normal = float3(input.normal_x, input.normal_y, input.normal_z);
  float4 in_tangent = float4(input.tangent_x, input.tangent_y, input.tangent_z, input.tangent_w);
  float2 in_uv = float2(input.uv_x, input.uv_y);

  uint4 in_bone_indices = uint4(input.bone_index_x, input.bone_index_y, input.bone_index_z, input.bone_index_w);
  float4 in_bone_weights = float4(input.bone_weight_x, input.bone_weight_y, input.bone_weight_z, input.bone_weight_w);

  float4x4 skinning_matrix = calculate_skinning_matrix(in_bone_indices, in_bone_weights, push.bone_matrices_buffer, push.bone_matrices_offset);

  float3 skinned_position = mul(skinning_matrix, float4(in_position, 1.0)).xyz;
  float3 skinned_normal = normalize(mul(skinning_matrix, float4(in_normal, 0.0)).xyz);
  float3 skinned_tangent = normalize(mul(skinning_matrix, float4(in_tangent.xyz, 0.0)).xyz);

  static_vertex output;

  output.position_x = skinned_position.x;
  output.position_y = skinned_position.y;
  output.position_z = skinned_position.z;

  output.normal_x = skinned_normal.x;
  output.normal_y = skinned_normal.y;
  output.normal_z = skinned_normal.z;

  output.uv_x = in_uv.x;
  output.uv_y = in_uv.y;

  output.tangent_x = skinned_tangent.x;
  output.tangent_y = skinned_tangent.y;
  output.tangent_z = skinned_tangent.z;
  output.tangent_w = in_tangent.w;

  push.static_vertices_buffer[id] = output;
}
