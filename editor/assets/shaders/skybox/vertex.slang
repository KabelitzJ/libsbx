#include <libsbx/math/constants.slang>
#include <libsbx/common/scene_data.slang>

struct vertex {
  float position_x;
  float position_y;
  float position_z;
}; // struct vertex

struct push_data {
  float4 tint;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  float3 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
  float4 sv_position : SV_Position;
}; // struct vs_out

static const float2 positions[3] = {
  float2(-1.0, -1.0),
  float2( 3.0, -1.0),
  float2(-1.0,  3.0)
};

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  float2 position = positions[input.vertex_id];

  float4 view_space = mul(scene.inverse_projection, float4(position, 1.0, 1.0));
  float3 view_dir   = view_space.xyz / view_space.w;

  output.uv = mul((float3x3)scene.inverse_view, view_dir);
  output.color = scene.light_color * push.tint;
  output.sv_position = float4(position, 1.0, 1.0);

  return output;
}
