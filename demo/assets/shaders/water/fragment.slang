#include <libsbx/common/scene_data.slang>
#include <libsbx/math/constants.slang>

#include <water/common.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct fs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerCube<float4> irradiance_image;

[[vk::binding(2, 0)]]
SamplerCube<float4> prefiltered_image;

static const float PREFILTERED_ENV_MAX_MIP = 9.0;
static const float3 WATER_F0 = float3(0.02, 0.02, 0.02);

float sample_terrain_height(float world_x, float world_z) {
  float grid_x = (world_x + push.terrain_offset_x) / push.base_cell_size;
  float grid_z = (world_z + push.terrain_offset_z) / push.base_cell_size;

  int x0 = int(floor(grid_x));
  int z0 = int(floor(grid_z));
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  x0 = clamp(x0, 0, int(push.vertices_x) - 1);
  x1 = clamp(x1, 0, int(push.vertices_x) - 1);
  z0 = clamp(z0, 0, int(push.vertices_z) - 1);
  z1 = clamp(z1, 0, int(push.vertices_z) - 1);

  float fx = grid_x - floor(grid_x);
  float fz = grid_z - floor(grid_z);

  float h00 = push.height_data_buffer[z0 * push.vertices_x + x0];
  float h10 = push.height_data_buffer[z0 * push.vertices_x + x1];
  float h01 = push.height_data_buffer[z1 * push.vertices_x + x0];
  float h11 = push.height_data_buffer[z1 * push.vertices_x + x1];

  return lerp(lerp(h00, h10, fx), lerp(h01, h11, fx), fz);
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float terrain_h = sample_terrain_height(input.position.x, input.position.z);
  float water_depth = push.water_y - terrain_h;

  if (water_depth < 0.05) {
    discard;
  }

  float depth_t = saturate(water_depth / push.depth_fade_distance);
  float3 base_color = lerp(push.water_color_shallow.rgb, push.water_color_deep.rgb, depth_t);

  float3 N = normalize(input.normal);
  float3 V = normalize(scene.camera_position.xyz - input.position);
  float NdotV = max(dot(N, V), 0.001);

  float fresnel_t = pow(1.0 - NdotV, 5.0);
  float3 F = WATER_F0 + (1.0 - WATER_F0) * fresnel_t;

  float3 R = reflect(-V, N);
  float lod = push.roughness * PREFILTERED_ENV_MAX_MIP;
  float3 sky_reflection = prefiltered_image.SampleLevel(R, lod).rgb;

  float3 ambient_irradiance = irradiance_image.Sample(N).rgb;
  float3 diffuse_lit = base_color * ambient_irradiance / sbx::math::pi;

  float3 sun_direction = normalize(-scene.light_direction);
  float3 H = normalize(V + sun_direction);
  float NdotH = max(dot(N, H), 0.0);
  float glint = pow(NdotH, 256.0);
  float3 sun_specular = scene.light_color.rgb * scene.light_color.a * glint;

  float3 final_color = lerp(diffuse_lit, sky_reflection, F) + sun_specular * F;

  float alpha_body = lerp(0.35, 0.85, depth_t);
  float alpha = lerp(alpha_body, 1.0, fresnel_t);
  alpha = saturate(alpha);

  float view_z = -mul(scene.view, float4(input.position, 1.0)).z;
  float weight = clamp(0.03 / (1e-5 + pow(view_z * 0.01, 4.0)), 1e-2, 3e3);

  output.accum = float4(final_color * alpha, alpha) * weight;
  output.revealage = alpha;

  return output;
}
