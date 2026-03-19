#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 world_xz : WORLD_XZ;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct fs_out

struct push_data {
  float* height_data_buffer;
  float origin_x;
  float origin_z;
  float plane_size;
  float water_level;
  uint grid_verts;
  uint grid_cells;
  float terrain_offset_x;
  float terrain_offset_z;
  float base_cell_size;
  uint vertices_x;
  uint vertices_z;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

// Golden-angle-spaced wave directions to avoid grid interference
float3 compute_wave_normal(float2 xz, float time, float depth_mask) {
  static const int WAVE_COUNT = 7;

  static const float dir_x[7]  = {  0.927,  0.515, -0.105, -0.682, -0.985, -0.891, -0.438 };
  static const float dir_z[7]  = {  0.375,  0.857,  0.995,  0.731,  0.174, -0.454, -0.899 };
  static const float freq[7]   = {  0.618,  1.0,    1.618,  2.618,  4.236,  6.854,  11.09 };
  static const float amp[7]    = {  0.25,   0.175,  0.1,    0.06,   0.035,  0.02,   0.01 };
  static const float speed[7]  = {  0.618,  1.0,    0.786,  1.272,  0.834,  1.106,  0.924 };

  float dx = 0.0;
  float dz = 0.0;

  for (int i = 0; i < WAVE_COUNT; i++) {
    float d = dir_x[i] * xz.x + dir_z[i] * xz.y;
    float phase = d * freq[i] + time * speed[i];
    float c = cos(phase) * freq[i] * amp[i];

    dx += dir_x[i] * c;
    dz += dir_z[i] * c;
  }

  // Dampen waves in shallow water
  dx *= depth_mask;
  dz *= depth_mask;

  return normalize(float3(-dx, 1.0, -dz));
}

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

  float time = scene.time;

  float terrain_h = sample_terrain_height(input.world_xz.x, input.world_xz.y);
  float water_depth = push.water_level - terrain_h;

  float shore_fade = smoothstep(0.0, 0.6, water_depth);
  float depth_mask = smoothstep(0.0, 3.0, water_depth);

  // Wave normals dampened in shallows
  float3 normal = compute_wave_normal(input.world_xz, time, depth_mask);

  float3 view_dir = normalize(scene.camera_position.xyz - input.position);
  float3 sun_dir = normalize(scene.light_direction.xyz);

  float n_dot_v = max(dot(normal, view_dir), 0.0);
  float fresnel = 0.02 + 0.98 * pow(1.0 - n_dot_v, 5.0);

  static const float3 SHALLOW_COLOR = float3(0.12, 0.28, 0.35);
  static const float3 DEEP_COLOR = float3(0.04, 0.10, 0.22);
  static const float3 SKY_TINT = float3(0.45, 0.60, 0.75);

  float depth_factor = clamp(water_depth / 8.0, 0.0, 1.0);
  float3 color = lerp(SHALLOW_COLOR, DEEP_COLOR, depth_factor);
  color = lerp(color, SKY_TINT, fresnel * 0.5);

  // Diffuse
  float n_dot_l = max(dot(normal, sun_dir), 0.0);
  color *= (0.35 + 0.65 * n_dot_l);

  // Specular
  float3 half_vec = normalize(view_dir + sun_dir);
  float spec = pow(max(dot(normal, half_vec), 0.0), 256.0);
  color += scene.light_color.rgb * spec * 0.6;

  float alpha = clamp(0.92 * shore_fade, 0.0, 0.99);

  // WBOIT
  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);
  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accum = float4(color * alpha, alpha) * weight;
  output.revealage = alpha;

  return output;
}
