#include <libsbx/math/algorithm.slang>

#include <grid/config.slang>

struct fs_in {
  float2 uv : TEXCOORD0;
  float2 camera_position  : TEXCOORD1;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

float4 grid_color(float2 uv, float2 camera_position) {
  float2 dudv = float2(length(float2(ddx(uv.x), ddy(uv.x))), length(float2(ddx(uv.y), ddy(uv.y))));

  float lod_level = max(0.0, log10((length(dudv) * grid_min_pixels_between_cells) / grid_cell_size) + 1.0);
  float lod_fade = frac(lod_level);

  float lod0 = grid_cell_size * pow(10.0, floor(lod_level));
  float lod1 = lod0 * 10.0;
  float lod2 = lod1 * 10.0;

  dudv *= 4.0;
  uv += dudv * 0.5;

  float lod0a = sbx::math::max2(float2(1.0, 1.0) - abs(sbx::math::satv(sbx::math::mod(uv, lod0) / dudv) * 2.0 - float2(1.0, 1.0)));
  float lod1a = sbx::math::max2(float2(1.0, 1.0) - abs(sbx::math::satv(sbx::math::mod(uv, lod1) / dudv) * 2.0 - float2(1.0, 1.0)));
  float lod2a = sbx::math::max2(float2(1.0, 1.0) - abs(sbx::math::satv(sbx::math::mod(uv, lod2) / dudv) * 2.0 - float2(1.0, 1.0)));

  float4 color = (lod2a > 0.0) ? grid_color_thick : (lod1a > 0.0) ? lerp(grid_color_thick, grid_color_thin, lod_fade) : grid_color_thin;

  uv -= camera_position;
  float opacity_falloff = (1.0 - sbx::math::satf(length(uv) / grid_size));

  color.a *= (lod2a > 0.0) ? lod2a : (lod1a > 0.0) ? lod1a : (lod0a * (1.0 - lod_fade));
  color.a *= opacity_falloff;

  return color;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  output.color = grid_color(input.uv, input.camera_position);

  return output;
}
