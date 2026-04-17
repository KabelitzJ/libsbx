#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

#include <terrain/common.slang>

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

static const uint LAYER_GRASS = 0;
static const uint LAYER_DIRT = 1;
static const uint LAYER_ROCK = 2;
static const uint LAYER_SAND = 3;
static const uint LAYER_SNOW = 4;
static const uint LAYER_MUD = 5;

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
Sampler2D<float4> noise_image;

[[vk::binding(2, 0)]]
SamplerState terrain_sampler;

[[vk::binding(3, 0)]]
Texture2D<float4> terrain_textures[sbx::common::max_image_array_size];

struct unpacked_weights {
  float grass;
  float dirt;
  float rock;
  float sand;
  float snow;
  float mud;
}; // struct unpacked_weights

float unpack_weight(uint8_t value) {
  return float(value) / 255.0;
}

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

unpacked_weights sample_splat_bilinear(float3 world_position) {
  float grid_x = (world_position.x + push.terrain_offset_x) / push.base_cell_size - 0.5;
  float grid_z = (world_position.z + push.terrain_offset_z) / push.base_cell_size - 0.5;

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

float3 sample_layer_blended(uint layer, float2 uv, float distance_factor) {
  float3 detail = terrain_textures[layer].Sample(terrain_sampler, uv).rgb;
  float3 far_sample = terrain_textures[layer].Sample(terrain_sampler, uv * 0.1).rgb;

  return lerp(detail, far_sample, distance_factor);
}

float3 sample_layer(uint layer, float2 uv) {
  return terrain_textures[layer].Sample(terrain_sampler, uv).rgb;
}

float3 sample_layer_triplanar(uint layer, float3 world_position, float3 blend) {
  float2 uv_xz = world_position.xz * push.tiling_scale;
  float2 uv_xy = world_position.xy * push.tiling_scale;
  float2 uv_zy = world_position.zy * push.tiling_scale;

  float3 col_xz = terrain_textures[layer].Sample(terrain_sampler, uv_xz).rgb;
  float3 col_xy = terrain_textures[layer].Sample(terrain_sampler, uv_xy).rgb;
  float3 col_zy = terrain_textures[layer].Sample(terrain_sampler, uv_zy).rgb;

  return col_xz * blend.y + col_xy * blend.z + col_zy * blend.x;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 normal = normalize(input.normal);
  unpacked_weights splat = sample_splat_bilinear(input.position);

  float camera_distance = length(input.position - scene.camera_position.xyz);
  float distance_factor = saturate(camera_distance / 200.0);

  float2 uv = input.position.xz * push.tiling_scale;

  float3 tex_grass = sample_layer_blended(LAYER_GRASS, uv, distance_factor);
  float3 tex_dirt  = sample_layer_blended(LAYER_DIRT, uv, distance_factor);
  float3 tex_sand  = sample_layer_blended(LAYER_SAND, uv, distance_factor);
  float3 tex_snow  = sample_layer_blended(LAYER_SNOW, uv, distance_factor);
  float3 tex_mud   = sample_layer_blended(LAYER_MUD, uv, distance_factor);

  // Tri-planar rock only on close rings to save 2 extra samples at distance
  float3 tex_rock;

  if (push.ring_level <= 1) {
    float3 blend = abs(normal);
    blend = pow(blend, 4.0);
    blend /= (blend.x + blend.y + blend.z);
    float3 detail = sample_layer_triplanar(LAYER_ROCK, input.position, blend);
    float3 far_sample = sample_layer_triplanar(LAYER_ROCK, input.position * 0.1, blend);
    tex_rock = lerp(detail, far_sample, distance_factor);
  } else {
    tex_rock = sample_layer_blended(LAYER_ROCK, uv, distance_factor);
  }

  float3 albedo = tex_grass * splat.grass
              + tex_dirt  * splat.dirt
              + tex_rock  * splat.rock
              + tex_sand  * splat.sand
              + tex_snow  * splat.snow
              + tex_mud   * splat.mud;

  // Macro variation — large-scale color breakup
  float macro = noise_image.Sample(input.position.xz * 0.002).r;
  albedo *= lerp(0.8, 1.0, macro);

  float roughness = 0.85 * splat.grass
                  + 0.95 * splat.dirt
                  + 0.75 * splat.rock
                  + 0.90 * splat.sand
                  + 0.60 * splat.snow
                  + 0.98 * splat.mud;

  output.albedo = float4(albedo, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normal * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, roughness, 1.0, 1.0);
  output.emissive = float4(0.0, 0.0, 0.0, 0.1);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
