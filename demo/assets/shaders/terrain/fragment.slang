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
  uint chunk_verts;
  uint chunk_cells;
  float cell_size;
  float chunk_world_x;
  float chunk_world_z;
  float inv_chunk_verts;
  float tiling_scale;
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
  float fx = (world_pos.x - push.chunk_world_x) / push.cell_size - 0.5;
  float fz = (world_pos.z - push.chunk_world_z) / push.cell_size - 0.5;

  int x0 = int(floor(fx));
  int y0 = int(floor(fz));
  int x1 = x0 + 1;
  int y1 = y0 + 1;

  float wx = fract(fx);
  float wy = fract(fz);

  x0 = clamp(x0, 0, int(push.chunk_cells) - 1);
  x1 = clamp(x1, 0, int(push.chunk_cells) - 1);
  y0 = clamp(y0, 0, int(push.chunk_cells) - 1);
  y1 = clamp(y1, 0, int(push.chunk_cells) - 1);

  unpacked_weights s00 = unpack_splat(push.splat_data_buffer[y0 * int(push.chunk_cells) + x0]);
  unpacked_weights s10 = unpack_splat(push.splat_data_buffer[y0 * int(push.chunk_cells) + x1]);
  unpacked_weights s01 = unpack_splat(push.splat_data_buffer[y1 * int(push.chunk_cells) + x0]);
  unpacked_weights s11 = unpack_splat(push.splat_data_buffer[y1 * int(push.chunk_cells) + x1]);

  unpacked_weights result;

  result.grass = lerp(lerp(s00.grass, s10.grass, wx), lerp(s01.grass, s11.grass, wx), wy);
  result.dirt = lerp(lerp(s00.dirt, s10.dirt, wx), lerp(s01.dirt, s11.dirt, wx), wy);
  result.rock = lerp(lerp(s00.rock, s10.rock, wx), lerp(s01.rock, s11.rock, wx), wy);
  result.sand = lerp(lerp(s00.sand, s10.sand, wx), lerp(s01.sand, s11.sand, wx), wy);
  result.snow = lerp(lerp(s00.snow, s10.snow, wx), lerp(s01.snow, s11.snow, wx), wy);
  result.mud = lerp(lerp(s00.mud, s10.mud, wx), lerp(s01.mud, s11.mud, wx), wy);

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
  output.material = float4(0.0, roughness, 1.0, 1.0);
  output.emissive = float4(0.0, 0.0, 0.0, 1.0);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
