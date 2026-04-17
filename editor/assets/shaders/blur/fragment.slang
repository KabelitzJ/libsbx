#include <libsbx/math/constants.slang>

[[vk::binding(0, 0)]] 
Sampler2D image;

struct push_data {
  float2 direction;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

// blur type constants
static const uint BLUR_TYPE_GAUSSIAN_5  = 0;
static const uint BLUR_TYPE_GAUSSIAN_9  = 1;
static const uint BLUR_TYPE_GAUSSIAN_13 = 2;

[[vk::constant_id(0)]]
const uint blur_type = BLUR_TYPE_GAUSSIAN_5;

float4 gaussian_blur_5(float2 uv, float2 resolution, float2 direction) {
  float4 color = float4(0.0, 0.0, 0.0, 0.0);

  float2 off1   = 1.3333333333333333 * direction;
  float2 texel1 = off1 / resolution;

  color += image.Sample(uv) * 0.29411764705882354;
  color += image.Sample(uv + texel1) * 0.35294117647058826;
  color += image.Sample(uv - texel1) * 0.35294117647058826;

  return color;
}

float4 gaussian_blur_9(float2 uv, float2 resolution, float2 direction) {
  float4 color = float4(0.0, 0.0, 0.0, 0.0);

  float2 off1   = 1.3846153846 * direction;
  float2 off2   = 3.2307692308 * direction;
  float2 texel1 = off1 / resolution;
  float2 texel2 = off2 / resolution;

  color += image.Sample(uv) * 0.2270270270;
  color += image.Sample(uv + texel1) * 0.3162162162;
  color += image.Sample(uv - texel1) * 0.3162162162;
  color += image.Sample(uv + texel2) * 0.0702702703;
  color += image.Sample(uv - texel2) * 0.0702702703;

  return color;
}

float4 gaussian_blur_13(float2 uv, float2 resolution, float2 direction) {
  float4 color = float4(0.0, 0.0, 0.0, 0.0);

  float2 off1   = 1.411764705882353  * direction;
  float2 off2   = 3.2941176470588234 * direction;
  float2 off3   = 5.176470588235294  * direction;

  float2 texel1 = off1 / resolution;
  float2 texel2 = off2 / resolution;
  float2 texel3 = off3 / resolution;

  color += image.Sample(uv) * 0.1964825501511404;
  color += image.Sample(uv + texel1) * 0.2969069646728344;
  color += image.Sample(uv - texel1) * 0.2969069646728344;
  color += image.Sample(uv + texel2) * 0.09447039785044732;
  color += image.Sample(uv - texel2) * 0.09447039785044732;
  color += image.Sample(uv + texel3) * 0.010381362401148057;
  color += image.Sample(uv - texel3) * 0.010381362401148057;

  return color;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  uint w, h;
  image.GetDimensions(w, h);

  float2 resolution = float2(w, h);

  float4 color = float4(1, 0, 1, 1);

  if (blur_type == BLUR_TYPE_GAUSSIAN_5) {
    color = gaussian_blur_5(input.uv, resolution, push.direction);
  } else if (blur_type == BLUR_TYPE_GAUSSIAN_9) {
    color = gaussian_blur_9(input.uv, resolution, push.direction);
  } else if (blur_type == BLUR_TYPE_GAUSSIAN_13) {
    color = gaussian_blur_13(input.uv, resolution, push.direction);
  }

  output.color = color;

  return output;
}
