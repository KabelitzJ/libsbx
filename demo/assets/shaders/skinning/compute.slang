struct cs_in {
  uint3 global_id : SV_DispatchThreadID;
  uint3 local_id : SV_GroupThreadID;
  uint3 group_id : SV_GroupID;
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
  float tangent_w;
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
  float tangent_w;

  uint bone_index_x;
  uint bone_index_y;
  uint bone_index_z;
  uint bone_index_w;

  float bone_weight_x;
  float bone_weight_y;
  float bone_weight_z;
  float bone_weight_w;
}; // struct skinned_vertex

struct skinning_job {
  skinned_vertex* pre_vertices;
  static_vertex* post_vertices;
  uint vertex_count;
  uint bone_offset;
}; // struct skinning_job

struct push_data {
  skinning_job* skinning_jobs;
  float4x4* bone_matrices_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

float4x4 calculate_skinning_matrix(uint4 bone_indices, float4 bone_weights, float4x4* bone_matrices, uint bone_offset) {
  float4x4 result = float4x4(
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0
  );

  [unroll]
  for (int i = 0; i < 4; ++i) {
    result += bone_weights[i] * bone_matrices[bone_offset + bone_indices[i]];
  }

  return result;
}

static const local_size_x = 64;

[shader("compute")]
[numthreads(local_size_x, 1, 1)]
void main(cs_in input) {
  uint job_id = input.group_id.x;
  uint block = input.group_id.y;
  uint local = input.local_id.x;

  skinning_job job = push.skinning_jobs[job_id];

  uint vertex_index = block * local_size_x + local;

  if (vertex_index >= job.vertex_count) {
    return;
  }

  skinned_vertex vin = job.pre_vertices[vertex_index];

  float3 in_position = float3(vin.position_x, vin.position_y, vin.position_z);
  float3 in_normal = float3(vin.normal_x, vin.normal_y, vin.normal_z);
  float4 in_tangent = float4(vin.tangent_x, vin.tangent_y, vin.tangent_z, vin.tangent_w);
  float2 in_uv = float2(vin.uv_x, vin.uv_y);

  uint4 bone_indices = uint4(vin.bone_index_x, vin.bone_index_y, vin.bone_index_z, vin.bone_index_w);
  float4 bone_weights = float4(vin.bone_weight_x, vin.bone_weight_y, vin.bone_weight_z, vin.bone_weight_w);

  float4x4 skinning_matrix = calculate_skinning_matrix(bone_indices, bone_weights, push.bone_matrices_buffer, job.bone_offset);

  float3 skinned_position = mul(skinning_matrix, float4(in_position, 1.0)).xyz;
  float3 skinned_normal = normalize(mul(skinning_matrix, float4(in_normal, 0.0)).xyz);
  float3 skinned_tangent = normalize(mul(skinning_matrix, float4(in_tangent.xyz, 0.0)).xyz);

  static_vertex vout;

  vout.position_x = skinned_position.x;
  vout.position_y = skinned_position.y;
  vout.position_z = skinned_position.z;

  vout.normal_x = skinned_normal.x;
  vout.normal_y = skinned_normal.y;
  vout.normal_z = skinned_normal.z;

  vout.uv_x = in_uv.x;
  vout.uv_y = in_uv.y;

  vout.tangent_x = skinned_tangent.x;
  vout.tangent_y = skinned_tangent.y;
  vout.tangent_z = skinned_tangent.z;
  vout.tangent_w = in_tangent.w;

  job.post_vertices[vertex_index] = vout;
}
