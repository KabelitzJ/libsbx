#include <libsbx/math/constants.slang>

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[[vk::binding(0, 0)]]
SamplerCube<float4> skybox;

[[vk::binding(1, 0)]]
RWTexture2DArray<float4> output;

static const uint sample_count = 4096u;
static const float firefly_clamp = 30.0f;

float3 cube_direction(float2 uv, uint side) {
  float2 tex = uv * 2.0 - 1.0;

  switch (side) {
    case 0: return float3( 1.0, -tex.y, -tex.x);
    case 1: return float3(-1.0, -tex.y,  tex.x);
    case 2: return float3( tex.x,  1.0,  tex.y);
    case 3: return float3( tex.x, -1.0, -tex.y);
    case 4: return float3( tex.x, -tex.y,  1.0);
    case 5: return float3(-tex.x, -tex.y, -1.0);
    default: return float3(1.0, 0.0, 0.0);
  }
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

float3 cosine_sample_hemisphere(float2 xi) {
  float phi = 2.0 * sbx::math::pi * xi.x;
  float cos_theta = sqrt(1.0 - xi.y);
  float sin_theta = sqrt(xi.y);

  return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height, layers;
  output.GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height || input.id.z >= 6u) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);
  float3 n = normalize(cube_direction(uv, input.id.z));

  float3 up = abs(n.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  float3 t = normalize(cross(up, n));
  float3 b = cross(n, t);

  float3 sum = float3(0, 0, 0);

  for (uint i = 0; i < sample_count; ++i) {
    float2 xi = hammersley(i, sample_count);
    float3 h_local = cosine_sample_hemisphere(xi);
    float3 direction = t * h_local.x + b * h_local.y + n * h_local.z;

    float3 color = min(skybox.SampleLevel(direction, 0.0).rgb, float3(firefly_clamp));
    sum += color;
  }

  float3 irradiance = sbx::math::pi * sum / float(sample_count);

  output[uint3(input.id.xy, input.id.z)] = float4(irradiance, 1.0);
}
