#include <libsbx/math/constants.slang>

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

struct push_data {
  float roughness;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
SamplerCube<float4> skybox;

[[vk::binding(1, 0)]]
RWTexture2DArray<float4> output;

static const uint SAMPLE_COUNT = 1024u;
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

float3 importance_sample_ggx(float2 Xi, float roughness, float3 N) {
  float alpha = roughness * roughness;

  float phi = 2.0 * sbx::math::pi * Xi.x;
  float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
  float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

  float3 H_local = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

  float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  float3 T = normalize(cross(up, N));
  float3 B = cross(N, T);

  return normalize(T * H_local.x + B * H_local.y + N * H_local.z);
}

float distribution_ggx(float n_dot_h, float roughness) {
  float alpha = roughness * roughness;
  float alpha2 = alpha * alpha;
  float denom = n_dot_h * n_dot_h * (alpha2 - 1.0) + 1.0;
  return alpha2 / (sbx::math::pi * denom * denom);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height, layers;
  output.GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint env_w, env_h, env_layers;
  skybox.GetDimensions(0, env_w, env_h, env_layers);
  float env_dim = float(env_w);

  float roughness = push.roughness;
  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);

  for (uint face = 0; face < 6; ++face) {
    float3 N = normalize(cube_direction(uv, face));
    float3 V = N;

    float3 prefiltered_color = float3(0, 0, 0);
    float total_weight = 0.0;

    if (roughness == 0.0) {
      // mirror reflection: no integration needed
      prefiltered_color = min(skybox.SampleLevel(N, 0.0).rgb, float3(FIREFLY_CLAMP));
      total_weight = 1.0;
    } else {
      for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importance_sample_ggx(Xi, roughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float n_dot_l = max(dot(N, L), 0.0);

        if (n_dot_l > 0.0) {
          // Lagarde filtered importance sampling: pick source mip
          float n_dot_h = max(dot(N, H), 0.0);
          float v_dot_h = max(dot(V, H), 0.0);
          float D = distribution_ggx(n_dot_h, roughness);
          float pdf = D * n_dot_h / (4.0 * v_dot_h) + 0.0001;

          float omega_s = 1.0 / (float(SAMPLE_COUNT) * pdf);
          float omega_p = 4.0 * sbx::math::pi / (6.0 * env_dim * env_dim);
          float mip = max(0.5 * log2(omega_s / omega_p), 0.0);

          float3 c = min(skybox.SampleLevel(L, mip).rgb, float3(FIREFLY_CLAMP));

          prefiltered_color += c * n_dot_l;
          total_weight += n_dot_l;
        }
      }

      prefiltered_color /= max(total_weight, 0.001);
    }

    output.Store(int3(input.id.xy, face), float4(prefiltered_color, 1.0));
  }
}
