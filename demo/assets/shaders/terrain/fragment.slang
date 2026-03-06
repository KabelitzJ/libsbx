#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
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

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 base_color = float3(0.35, 0.55, 0.25);

  output.albedo = float4(base_color, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, 1.0, 1.0, 1.0);
  output.emissive = float4(0.0, 0.0, 0.0, 1.0);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
