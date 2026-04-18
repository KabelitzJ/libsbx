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

static const float PI = 3.14159265358979323846f;
static const uint samples = 256u;

float3 cube_direction(float2 uv, uint side) {
  // [0, 1] -> [-1, 1]
  float2 tex = uv * 2.0f - 1.0f;

  if (side == 0) return float3( 1.0f, -tex.y, -tex.x); // Front
  if (side == 1) return float3(-1.0f, -tex.y,  tex.x); // Back
  if (side == 2) return float3( tex.x,  1.0f,  tex.y); // Right
  if (side == 3) return float3( tex.x, -1.0f, -tex.y); // Left
  if (side == 4) return float3( tex.x, -tex.y,  1.0f); // Top
  if (side == 5) return float3(-tex.x, -tex.y, -1.0f); // Bottom

  return float3(1.0f, 0.0f, 0.0f);
}

float random2(float2 co) {
  float a = 12.9898f;
  float b = 78.233f;
  float c = 43758.5453f;

  float dt = dot(co, float2(a, b));
  float sn = fmod(dt, 3.14f);

  return frac(sin(sn) * c);
}

float2 hammersley2d(uint i, uint N) {
  uint bits = (i << 16u) | (i >> 16u);

  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

  float rdi = float(bits) * 2.3283064365386963e-10f;
  return float2(float(i) / float(N), rdi);
}

float3 importance_sample_ggx(float2 Xi, float roughness, float3 normal) {
  float alpha = roughness * roughness;

  float phi = 2.0f * PI * Xi.x + random2(normal.xz) * 0.1f;
  float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (alpha * alpha - 1.0f) * Xi.y));
  float sinTheta = sqrt(1.f - cosTheta * cosTheta);

  float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

  float3 up = abs(normal.z) < 0.999f ? float3(0,0,1) : float3(1,0,0);
  float3 tangentX = normalize(cross(up, normal));
  float3 tangentY = normalize(cross(normal, tangentX));

  return normalize(H.x * tangentX + H.y * tangentY + H.z * normal);
}

float d_ggx(float n_dot_h, float roughness) {
  float alpha = roughness * roughness;
  float alpha2 = alpha * alpha;

  float denom = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;

  return alpha2 / (PI * denom * denom);
}

float3 prefilter_env_map(float3 R, float roughness) {
  float3 N = R;
  float3 V = R;

  float3 color = float3(0,0,0);
  float total_weight = 0;

  uint w, h, layers;
  output.GetDimensions(w, h, layers);

  uint env_w, env_h, env_layers;
  skybox.GetDimensions(0, env_w, env_h, env_layers);
  float env_map_dim = float(env_w);

  for (uint i = 0; i < samples; ++i) {
    float2 Xi = hammersley2d(i, samples);
    float3 H = importance_sample_ggx(Xi, roughness, N);
    float3 L = normalize(2.0f * dot(V, H) * H - V);

    float n_dot_l = max(dot(N, L), 0.0f);

    if (n_dot_l > 0) {
      float n_dot_h = max(dot(N, H), 0.0f);
      float v_dot_h = max(dot(V, H), 0.0f);

      float pdf = d_ggx(n_dot_h, roughness) * n_dot_h / (4.0 * v_dot_h) + 0.0001f;

      float omega_s = 1.0f / (float(samples) * pdf);
      float omega_p = 4.0f * PI / (6.0f * env_map_dim * env_map_dim);

      float mip_level = (roughness == 0.0f) ? 0.0f : max(0.5f * log2(omega_s / omega_p) + 1.0f, 0.0f);

      float3 c = skybox.SampleLevel(L, mip_level).rgb;

      color += c * n_dot_l;
      total_weight += n_dot_l;
    }
  }

  return color / max(total_weight, 0.001f);
}


[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint w, h, layers;
  output.GetDimensions(w, h, layers);

  if (input.id.x >= w || input.id.y >= h) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5f) / float2(w, h);

  for (uint face = 0; face < 6; ++face) {
    float3 R = normalize(cube_direction(uv, face));
    float3 c = prefilter_env_map(R, push.roughness);

    output.Store(int3(input.id.xy, face), float4(c, 1.0f));
  }
}
