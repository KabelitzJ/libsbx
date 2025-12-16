#include <libsbx/math/constants.slang>

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[[vk::binding(0, 0)]]
SamplerCube<float4> skybox;

[[vk::binding(1, 0)]]
RWTexture2DArray<float4> output;

static const float delta_phi = (2.0 * sbx::math::pi) / 180.0;
static const float delta_theta = (0.5 * sbx::math::pi) / 64.0;

float3 cube_direction(float2 uv, uint side) {
  float2 tex = uv * 2.0 - 1.0;

  if (side == 0) return float3( 1.0, -tex.y, -tex.x);
  if (side == 1) return float3(-1.0, -tex.y,  tex.x);
  if (side == 2) return float3( tex.x,  1.0,  tex.y);
  if (side == 3) return float3( tex.x, -1.0, -tex.y);
  if (side == 4) return float3( tex.x, -tex.y,  1.0);
  if (side == 5) return float3(-tex.x, -tex.y, -1.0);

  return float3(1.0, 1.0, 1.0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height, layers;
  output.GetDimensions(width, height, layers);

  float2 uv = (float2(input.id.xy) + float2(0.5)) / float2(width, height);

  for (uint face = 0; face < 6; ++face) {
    float3 N = normalize(cube_direction(uv, face));
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float3 color = float3(0.0, 0.0, 0.0);
    uint sample_count = 0;

    for (float phi = 0.0; phi < sbx::math::two_pi; phi += delta_phi) {
      for (float theta = 0.0; theta < sbx::math::half_pi; theta += delta_theta) {
        float3 direction = cos(phi) * right + sin(phi) * up;
        float3 uav = cos(theta) * N + sin(theta) * direction;

        float weight = cos(theta) * sin(theta);

        float3 sampled = skybox.Sample(uav).rgb;

        color += sampled * weight;
        sample_count++;
      }
    }

    float3 final_output = sbx::math::pi * color / float(sample_count);

    output.Store(int3(input.id.xy, face), float4(final_output, 1.0));
  }
}
