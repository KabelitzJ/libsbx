#include <libsbx/common/scene_data.slang>

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]] 
Sampler2D<float4> resolve_image;

[[vk::binding(2, 0)]] 
Sampler2D<uint> object_id_image;

[[vk::binding(3, 0)]] 
Sampler2D<float> linear_depth_image;

struct push_data {
  float4 color;     // rgb = outline color, a = max intensity
  float  thickness; // world-space thickness input scene units
};

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct fs_in {
  float2 uv : TEXCOORD0;
};

struct fs_out {
  float4 color : SV_Target0;
};

static const float GOLDEN_ANGLE = 2.39996323; // radians

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 base = resolve_image.Sample(input.uv);
  uint center_id = object_id_image.Sample(input.uv);
  float norm_depth = linear_depth_image.Sample(input.uv);

  if (center_id == 0u) {
    output.color = base;

    return output;
  }

  // World space thickness to pixel thickness
  float z_eye = scene.camera_near + norm_depth * (scene.camera_far - scene.camera_near);

  float pixels_per_unit = (scene.viewport.y * 0.5) / tan(scene.camera_fov_radians * 0.5);
  pixels_per_unit /= max(z_eye, 0.0001);

  float thickness_px = clamp(push.thickness * pixels_per_unit, 1.0, 40.0);

  uint width, height;
  object_id_image.GetDimensions(width, height);
  float2 texel = 1.0 / float2(width, height);

  // Spiral sampling
  // tweak: 24–48 is ideal
  const int NUM_SAMPLES = 32;
  float min_dist = 1e9;

  [unroll]
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    float t = float(i) / float(NUM_SAMPLES);

    float r = t * thickness_px;
    float a = i * GOLDEN_ANGLE;
    float2 dir = float2(cos(a), sin(a));

    float2 uv2 = input.uv + dir * (r * texel);
    uv2 = clamp(uv2, 0.0, 1.0);

    uint id2 = object_id_image.Sample(uv2);

    if (id2 != center_id) {
      min_dist = min(min_dist, r);
    }
  }

  if (min_dist > thickness_px) {
    output.color = base;

    return output;
  }

  // Fade inward
  float border = 1.0;
  float d = max(min_dist - border, 0.0);

  // 0 at border, 1 inside
  float t = d / max(thickness_px, 0.001);
  t = saturate(t);

  float strength = 1.0 - t;
  float alpha = strength * push.color.a;

  float3 final_rgb = lerp(base.rgb, push.color.rgb, alpha);
  output.color = float4(final_rgb, base.a);

  return output;
}
