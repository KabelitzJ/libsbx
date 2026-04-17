#include <libsbx/math/constants.slang>
#include <libsbx/math/algorithm.slang>

[[vk::binding(0, 0)]]
Sampler2D image;

struct fs_in {
  float2 uv : TEXCOORD0;
};

struct fs_out {
  float4 color : SV_Target0;
};

static const float kernel[49] = {
  0.00000067, 0.00002292, 0.00019117, 0.00038771, 0.00019117, 0.00002292, 0.00000067,
  0.00002292, 0.00078634, 0.00655965, 0.01330373, 0.00655965, 0.00078634, 0.00002292,
  0.00019117, 0.00655965, 0.05472157, 0.11116501, 0.05472157, 0.00655965, 0.00019117,
  0.00038771, 0.01330373, 0.11116501, 0.22668157, 0.11116501, 0.01330373, 0.00038771,
  0.00019117, 0.00655965, 0.05472157, 0.11116501, 0.05472157, 0.00655965, 0.00019117,
  0.00002292, 0.00078634, 0.00655965, 0.01330373, 0.00655965, 0.00078634, 0.00002292,
  0.00000067, 0.00002292, 0.00019117, 0.00038771, 0.00019117, 0.00002292, 0.00000067
};

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  uint w, h;
  image.GetDimensions(w, h);
  float2 texel = float2(1.0 / float(w), 1.0 / float(h));

  float3 sum = float3(0.0);
  int idx = 0;

  // kernel radius = 3 (7x7 kernel)
  for (int y = -3; y <= 3; ++y) {
    for (int x = -3; x <= 3; ++x) {
      float2 offset = float2(x, y) * texel;
      float3 sample = image.Sample(input.uv + offset).rgb;
      sum += sample * kernel[idx++];
    }
  }

  output.color = float4(sum, 1.0);
  return output;
}
