#include <grid/config.slang>

struct push_data {
  float4x4 mvp;
  float4 camera_position;
  float4 origin;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  float2 uv : TEXCOORD0;
  float2 camera_position : TEXCOORD1;
  float4 position : SV_Position;
}; // struct vs_out

static const float3 vertices[4] = {
  float3(-1.0, 0.0, -1.0),
  float3( 1.0, 0.0, -1.0),
  float3( 1.0, 0.0,  1.0),
  float3(-1.0, 0.0,  1.0)
};

static const int indices[6] = { 0, 1, 2, 2, 3, 0 };

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  int index = indices[input.vertex_id];

  float3 position = vertices[index] * grid_size;

  position.x += push.camera_position.x;
  position.z += push.camera_position.z;
  position += push.origin.xyz;

  output.camera_position = push.camera_position.xz;
  output.uv = position.xz;

  output.position = mul(push.mvp, float4(position, 1.0));

  return output;
}