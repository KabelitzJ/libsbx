#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  [[vk::location(2)]] nointerpolation uint zone_type : ZONE_TYPE;
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

struct zone_vertex_data {
  float position_x;
  float position_y;
  float position_z;
  float uv_u;
  float uv_v;
  uint zone_type;
}; // struct zone_vertex_data

struct push_data {
  zone_vertex_data* vertex_data_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float3 get_zone_color(uint type) {
  if (type == 1) return float3(0.2, 0.7, 0.3);   // residential - green
  if (type == 2) return float3(0.8, 0.6, 0.1);   // industrial - amber
  if (type == 3) return float3(0.6, 0.4, 0.2);   // extraction - brown
  if (type == 4) return float3(0.4, 0.8, 0.2);   // agricultural - lime
  if (type == 5) return float3(0.3, 0.4, 0.9);   // civic - blue
  if (type == 6) return float3(0.7, 0.2, 0.2);   // military - red
  if (type == 7) return float3(0.5, 0.5, 0.5);   // infrastructure - grey
  return float3(0.0, 0.0, 0.0);
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 zone_color = get_zone_color(input.zone_type);

  // Diagonal stripe pattern for planning overlay look
  float stripe = frac((input.uv.x + input.uv.y) * 3.0);
  float stripe_mask = smoothstep(0.45, 0.5, stripe) - smoothstep(0.7, 0.75, stripe);

  // Cell border lines
  float2 edge = abs(input.uv - 0.5) * 2.0;
  float border = max(smoothstep(0.85, 0.95, edge.x), smoothstep(0.85, 0.95, edge.y));

  // Combine: base zone tint + stripes + border
  float pattern = max(stripe_mask * 0.4, border * 0.6);
  float3 albedo = zone_color * lerp(0.35, 0.7, pattern);

  // Geometric normal from derivatives
  float3 dp_dx = ddx(input.position);
  float3 dp_dy = ddy(input.position);
  float3 normal = normalize(cross(dp_dx, dp_dy));

  if (normal.y < 0.0) {
    normal = -normal;
  }

  output.albedo = float4(albedo, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normal * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, 0.9, 1.0, 1.0);

  // Emissive glow so zones are visible in shadow
  output.emissive = float4(zone_color * 0.15, 1.0);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
