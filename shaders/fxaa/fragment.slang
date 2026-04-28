[[vk::binding(0, 0)]]
Sampler2D image;

struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

static const float EDGE_THRESHOLD_MIN = 0.0312;
static const float EDGE_THRESHOLD_MAX = 0.125;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  uint w, h;
  image.GetDimensions(w, h);

  float2 resolution = float2(w, h);
  float2 inv_resolution = 1.0 / resolution;

  float3 rgb_nw = image.Sample(input.uv + float2(-1.0, -1.0) * inv_resolution).xyz;
  float3 rgb_ne = image.Sample(input.uv + float2( 1.0, -1.0) * inv_resolution).xyz;
  float3 rgb_sw = image.Sample(input.uv + float2(-1.0,  1.0) * inv_resolution).xyz;
  float3 rgb_se = image.Sample(input.uv + float2( 1.0,  1.0) * inv_resolution).xyz;
  float3 rgb_m = image.Sample(input.uv).xyz;

  float3 luma_w = float3(0.2126, 0.7152, 0.0722);

  float luma_nw = sqrt(max(dot(rgb_nw, luma_w), 0.0));
  float luma_ne = sqrt(max(dot(rgb_ne, luma_w), 0.0));
  float luma_sw = sqrt(max(dot(rgb_sw, luma_w), 0.0));
  float luma_se = sqrt(max(dot(rgb_se, luma_w), 0.0));
  float luma_m = sqrt(max(dot(rgb_m,  luma_w), 0.0));

  float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
  float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));
  float luma_range = luma_max - luma_min;

  if (luma_range < max(EDGE_THRESHOLD_MIN, luma_max * EDGE_THRESHOLD_MAX)) {
    output.color = float4(rgb_m, 1.0);

    return output;
  }

  float2 direction = float2(-((luma_nw + luma_ne) - (luma_sw + luma_se)), ((luma_nw + luma_sw) - (luma_ne + luma_se)));

  float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * (0.25 * 0.25), 0.001);
  float rcp_direction_min = 1.0 / (min(abs(direction.x), abs(direction.y)) + dir_reduce);

  float2 min_direction = clamp(direction * rcp_direction_min, float2(-8.0, -8.0), float2( 8.0,  8.0)) * inv_resolution;

  float3 rgb_a = 0.5 * (image.Sample(input.uv + min_direction * (1.0 / 3.0 - 0.5)).xyz + image.Sample(input.uv + min_direction * (2.0 / 3.0 - 0.5)).xyz);

  float3 rgb_b = rgb_a * 0.5 + 0.25 * (image.Sample(input.uv + min_direction * -0.5).xyz + image.Sample(input.uv + min_direction *  0.5).xyz);

  float luma_b = sqrt(max(dot(rgb_b, luma_w), 0.0));

  output.color = ((luma_b < luma_min) || (luma_b > luma_max)) ? float4(rgb_a, 1.0) : float4(rgb_b, 1.0);

  return output;
}
