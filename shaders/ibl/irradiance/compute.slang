#include <libsbx/math/constants.slang>

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[[vk::binding(0, 0)]]
SamplerCube<float4> skybox;

[[vk::binding(1, 0)]]
RWTexture2DArray<float4> output;

static const uint SAMPLE_COUNT = 4096u;
static const float FIREFLY_CLAMP = 30.0f;

float3 cube_direction(float2 uv, uint side) {
  float2 tex = uv * 2.0 - 1.0;

  if (side == 0) return float3( 1.0, -tex.y, -tex.x);
  if (side == 1) return float3(-1.0, -tex.y,  tex.x);
  if (side == 2) return float3( tex.x,  1.0,  tex.y);
  if (side == 3) return float3( tex.x, -1.0, -tex.y);
  if (side == 4) return float3( tex.x, -tex.y,  1.0);
  if (side == 5) return float3(-tex.x, -tex.y, -1.0);

  return float3(1.0, 0.0, 0.0);
}

float radical_inverse_vdc(uint bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10f;
}

float2 hammersley(uint i, uint n) {
  return float2(float(i) / float(n), radical_inverse_vdc(i));
}

float3 cosine_sample_hemisphere(float2 Xi) {
  float phi = 2.0 * sbx::math::pi * Xi.x;
  float cos_theta = sqrt(1.0 - Xi.y);
  float sin_theta = sqrt(Xi.y);
  return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height, layers;
  output.GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);

  for (uint face = 0; face < 6; ++face) {
    float3 N = normalize(cube_direction(uv, face));

    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float3 sum = float3(0, 0, 0);

    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
      float2 Xi = hammersley(i, SAMPLE_COUNT);
      float3 H_local = cosine_sample_hemisphere(Xi);
      float3 dir = T * H_local.x + B * H_local.y + N * H_local.z;

      float3 c = min(skybox.SampleLevel(dir, 0.0).rgb, float3(FIREFLY_CLAMP));
      sum += c;
    }

    // E ≈ (π/N) Σ L  for cosine-weighted hemisphere sampling
    float3 irradiance = sbx::math::pi * sum / float(SAMPLE_COUNT);

    output.Store(int3(input.id.xy, face), float4(irradiance, 1.0));
  }
}
