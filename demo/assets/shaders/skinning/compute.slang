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
  float4x4* bone_matrices_buffer;
  static_vertex* static_vertices_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height;
  output.GetDimensions(width, height);

  float2 uv = (float2(input.id.xy) + float2(0.5)) / float2(width, height);
  float2 result = brdf(uv.x, 1.0 - uv.y);

  output.Store(input.id.xy, result);
}
