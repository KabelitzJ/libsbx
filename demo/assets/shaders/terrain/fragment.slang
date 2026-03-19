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

struct splat_weights {
  uint8_t grass;
  uint8_t dirt;
  uint8_t rock;
  uint8_t sand;
  uint8_t snow;
  uint8_t mud;
  uint8_t _padding0;
  uint8_t _padding1;
}; // struct splat_weights

struct push_data {
  float* height_data_buffer;
  splat_weights* splat_data_buffer;
  uint grid_vertices;
  uint grid_cells;
  float ring_cell_size;
  float ring_origin_x;
  float ring_origin_z;
  uint ring_level;
  float base_cell_size;
  float terrain_offset_x;
  float terrain_offset_z;
  float terrain_width;
  float terrain_height;
  uint vertices_x;
  uint vertices_z;
  uint cells_x;
  uint cells_z;
  float inner_min_x;
  float inner_min_z;
  float inner_max_x;
  float inner_max_z;
  uint ring_count;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

static const float3 COLOR_GRASS = float3(0.30, 0.50, 0.20);
static const float3 COLOR_DIRT = float3(0.45, 0.35, 0.22);
static const float3 COLOR_ROCK = float3(0.50, 0.48, 0.45);
static const float3 COLOR_SAND = float3(0.76, 0.70, 0.50);
static const float3 COLOR_SNOW = float3(0.90, 0.91, 0.93);
static const float3 COLOR_MUD = float3(0.30, 0.25, 0.18);

float unpack_weight(uint8_t value) {
  return float(value) / 255.0;
}

struct unpacked_weights {
  float grass;
  float dirt;
  float rock;
  float sand;
  float snow;
  float mud;
}; // struct unpacked_weights

unpacked_weights unpack_splat(splat_weights packed) {
  unpacked_weights result;

  result.grass = unpack_weight(packed.grass);
  result.dirt = unpack_weight(packed.dirt);
  result.rock = unpack_weight(packed.rock);
  result.sand = unpack_weight(packed.sand);
  result.snow = unpack_weight(packed.snow);
  result.mud = unpack_weight(packed.mud);

  return result;
}

unpacked_weights sample_splat_bilinear(float3 world_pos) {
  float grid_x = (world_pos.x + push.terrain_offset_x) / push.base_cell_size - 0.5;
  float grid_z = (world_pos.z + push.terrain_offset_z) / push.base_cell_size - 0.5;

  int x0 = int(floor(grid_x));
  int z0 = int(floor(grid_z));
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  float fraction_x = fract(grid_x);
  float fraction_z = fract(grid_z);

  x0 = clamp(x0, 0, int(push.cells_x) - 1);
  x1 = clamp(x1, 0, int(push.cells_x) - 1);
  z0 = clamp(z0, 0, int(push.cells_z) - 1);
  z1 = clamp(z1, 0, int(push.cells_z) - 1);

  unpacked_weights s00 = unpack_splat(push.splat_data_buffer[z0 * int(push.cells_x) + x0]);
  unpacked_weights s10 = unpack_splat(push.splat_data_buffer[z0 * int(push.cells_x) + x1]);
  unpacked_weights s01 = unpack_splat(push.splat_data_buffer[z1 * int(push.cells_x) + x0]);
  unpacked_weights s11 = unpack_splat(push.splat_data_buffer[z1 * int(push.cells_x) + x1]);

  unpacked_weights result;

  result.grass = lerp(lerp(s00.grass, s10.grass, fraction_x), lerp(s01.grass, s11.grass, fraction_x), fraction_z);
  result.dirt = lerp(lerp(s00.dirt, s10.dirt, fraction_x), lerp(s01.dirt, s11.dirt, fraction_x), fraction_z);
  result.rock = lerp(lerp(s00.rock, s10.rock, fraction_x), lerp(s01.rock, s11.rock, fraction_x), fraction_z);
  result.sand = lerp(lerp(s00.sand, s10.sand, fraction_x), lerp(s01.sand, s11.sand, fraction_x), fraction_z);
  result.snow = lerp(lerp(s00.snow, s10.snow, fraction_x), lerp(s01.snow, s11.snow, fraction_x), fraction_z);
  result.mud = lerp(lerp(s00.mud, s10.mud, fraction_x), lerp(s01.mud, s11.mud, fraction_x), fraction_z);

  return result;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  unpacked_weights splat = sample_splat_bilinear(input.position);

  float3 albedo = COLOR_GRASS * splat.grass + COLOR_DIRT * splat.dirt + COLOR_ROCK * splat.rock + COLOR_SAND * splat.sand + COLOR_SNOW * splat.snow + COLOR_MUD * splat.mud;
  float roughness = 0.85 * splat.grass + 0.95 * splat.dirt + 0.75 * splat.rock + 0.90 * splat.sand + 0.60 * splat.snow + 0.98 * splat.mud;

  output.albedo = float4(albedo, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, roughness, 1.0, 0.0);
  output.emissive = float4(0.0, 0.0, 0.0, 0.25);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
