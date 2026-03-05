#ifndef SPRITES_COMMON_SLANG_
#define SPRITES_COMMON_SLANG_

static const uint flag_billboard = 0x00000001u;
static const uint flag_msdf_text = 0x00000002u;

struct sprite_data {
  float4x4 model;
  float4 base_color;
  float4 emissive_factor;
  float2 size;
  float2 pivot;
  float2 uv_min;
  float2 uv_max;
  float emissive_strength;
  uint albedo_image_index;
  uint emissive_image_index;
  int sort_order;
  uint flags;
  float sdf_px_range;
  uint padding0;
  uint padding1;
}; // struct sprite_data

#endif // SPRITES_COMMON_SLANG_
