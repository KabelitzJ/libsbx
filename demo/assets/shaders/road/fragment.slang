#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  [[vk::location(2)]] nointerpolation uint road_type : ROAD_TYPE;
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

struct road_vertex_data {
  float position_x;
  float position_y;
  float position_z;
  float uv_u;
  float uv_v;
  uint road_type;
}; // struct road_vertex_data

struct push_data {
  road_vertex_data* vertex_data_buffer;
  uint preview_mode;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerState road_sampler;

[[vk::binding(2, 0)]]
Texture2D<float4> road_textures[sbx::common::max_image_array_size];

float get_road_roughness(uint type) {
  if (type == 1) return 0.95;
  if (type == 2) return 0.85;
  if (type == 3) return 0.55;
  if (type == 4) return 0.45;
  return 1.0;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  uint road_type = input.road_type;

  float tile_scale = 0.5;
  float2 tex_uv = float2(input.uv.x, input.uv.y * tile_scale);

  // Indices 0..3 = albedo, 4..7 = normal
  uint layer = min(road_type, 4u) - 1u;
  float3 tex_color = road_textures[layer].Sample(road_sampler, tex_uv).rgb;

  // Normal map (tangent space)
  float3 normal_sample = road_textures[layer + 4u].Sample(road_sampler, tex_uv).rgb;
  normal_sample = normal_sample * 2.0 - 1.0;

  // TBN from screen-space derivatives of position and UV
  float3 dp_dx = ddx(input.position);
  float3 dp_dy = ddy(input.position);
  float2 duv_dx = ddx(input.uv);
  float2 duv_dy = ddy(input.uv);

  float inv_det = 1.0 / (duv_dx.x * duv_dy.y - duv_dx.y * duv_dy.x);

  float3 tangent = normalize((dp_dx * duv_dy.y - dp_dy * duv_dx.y) * inv_det);
  float3 bitangent = normalize((dp_dy * duv_dx.x - dp_dx * duv_dy.x) * inv_det);

  float3 geom_normal = normalize(cross(dp_dx, dp_dy));

  if (geom_normal.y < 0.0) {
    geom_normal = -geom_normal;
  }

  // Re-orthogonalize
  tangent = normalize(tangent - geom_normal * dot(geom_normal, tangent));
  bitangent = normalize(cross(geom_normal, tangent));

  float3 normal = normalize(tangent * normal_sample.x + bitangent * normal_sample.y + geom_normal * normal_sample.z);

  // Edge darkening
  float road_roughness = get_road_roughness(road_type);
  float edge_distance = abs(input.uv.x - 0.5) * 2.0;
  float edge_darken = smoothstep(0.7, 1.0, edge_distance);
  float3 albedo = tex_color * lerp(1.0, 0.7, edge_darken);

  output.albedo = float4(albedo, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normal * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, road_roughness, 1.0, 1.0);

  // Preview: green emissive overlay
  float3 emissive_color = float3(0.0, 0.0, 0.0);

  if (push.preview_mode == 1u) {
    emissive_color = float3(0.15, 0.6, 0.15);
    output.albedo.rgb = lerp(albedo, float3(0.2, 0.5, 0.2), 0.4);
  }

  output.emissive = float4(emissive_color, 1.0);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
