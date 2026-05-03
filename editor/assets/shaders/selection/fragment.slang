[[vk::binding(0, 0)]] 
Sampler2D<float4> resolve_image;

[[vk::binding(1, 0)]] 
Sampler2D<uint> object_id_image;

[[vk::binding(2, 0)]] 
Sampler2D<float> linear_depth_image;

struct push_data {
  float4 color;
  float thickness;
  uint selected_id;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

static const float GOLDEN_ANGLE = 2.39996323;
static const float DEPTH_TOLERANCE = 0.0;
static const uint INVALID_ID = 0xFFFFFFFFu;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 base = resolve_image.Sample(input.uv);
  uint center_id = object_id_image.Sample(input.uv);
  float center_depth = linear_depth_image.Sample(input.uv);

  if (push.selected_id == INVALID_ID) {
    output.color = base;
    return output;
  }

  float thickness_px = push.thickness;

  uint width, height;
  object_id_image.GetDimensions(width, height);
  float2 texel = 1.0 / float2(width, height);

  const bool center_is_selected = (center_id == push.selected_id);

  const int NUM_SAMPLES = 32;
  float min_dist = 1e9;
  bool found_selected = center_is_selected;

  [unroll]
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    float t = float(i) / float(NUM_SAMPLES);

    float r = t * thickness_px;
    float a = i * GOLDEN_ANGLE;
    float2 dir = float2(cos(a), sin(a));

    float2 uv2 = input.uv + dir * (r * texel);
    uv2 = clamp(uv2, 0.0, 1.0);

    uint id2 = object_id_image.Sample(uv2);
    float depth2 = linear_depth_image.Sample(uv2);

    bool sample_is_selected = (id2 == push.selected_id);

    if (sample_is_selected) {
      found_selected = true;
    }

    if (center_is_selected != sample_is_selected) {
      float selected_depth = center_is_selected ? center_depth : depth2;
      float other_depth = center_is_selected ? depth2 : center_depth;

      if (other_depth >= selected_depth - DEPTH_TOLERANCE) {
        min_dist = min(min_dist, r);
      }
    }
  }

  if (!found_selected) {
    output.color = base;
    return output;
  }

  if (!center_is_selected) {
    if (min_dist > thickness_px) {
      output.color = base;
      return output;
    }
  }

  float border = 1.0;
  float d = max(min_dist - border, 0.0);

  float t = d / max(thickness_px, 0.001);
  t = saturate(t);

  float strength = 1.0 - t;
  float alpha = strength * push.color.a;

  float3 final_rgb = lerp(base.rgb, push.color.rgb, alpha);
  output.color = float4(final_rgb, base.a);

  return output;
}
