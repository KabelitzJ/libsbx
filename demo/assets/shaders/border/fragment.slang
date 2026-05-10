#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>

#include <border/common.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 ribbon_uv : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 albedo : SV_Target0;
  float4 position : SV_Target1;
  float4 normal : SV_Target2;
  float4 material : SV_Target3;
  float4 emissive : SV_Target4;
  uint object_id : SV_Target5;
  float linear_depth : SV_Target6;
}; // struct fs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  // ribbon_uv.y is in [-1, 1] across the ribbon width
  float across = abs(input.ribbon_uv.y);

  // anti-alias the edge: across=0 fully opaque, across=1 fully transparent
  float aa = fwidth(across) + 1e-5;
  float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, across);

  clip(alpha - 0.001);

  output.albedo = float4(push.color.rgb * alpha, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, 0.95, 1.0, 1.0);
  output.emissive = float4(0.0, 0.0, 0.0, 0.05);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
