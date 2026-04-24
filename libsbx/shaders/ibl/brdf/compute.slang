struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[[vk::binding(0, 0)]]
RWTexture2D<float2> output;

static const float PI = 3.1415926535897932384626433832795;
static const uint NUM_SAMPLES = 1024u;

float random(float2 co) {
  float a = 12.9898;
  float b = 78.233;
  float c = 43758.5453;
  float dt = dot(co, float2(a, b));
  float sn = fmod(dt, 3.14);

  return frac(sin(sn) * c);
}

float2 hammersley_2d(uint i, uint n) {
  uint bits = (i << 16u) | (i >> 16u);

  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

  float rdi = float(bits) * 2.3283064365386963e-10;

  return float2(float(i) / float(n), rdi);
}

float3 importance_sample_ggx(float2 xi, float roughness, float3 normal) {
  float alpha = roughness * roughness;
  float phi = 2.0 * PI * xi.x + random(normal.xz) * 0.1;
  float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
  float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

  float3 h = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
  float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  float3 tangent_x = normalize(cross(up, normal));
  float3 tangent_y = normalize(cross(normal, tangent_x));

  return normalize(tangent_x * h.x + tangent_y * h.y + normal * h.z);
}

float g_schlicksmith_ggx(float dot_nl, float dot_nv, float roughness) {
  float k = (roughness * roughness) * 0.5;
  float gl = dot_nl / (dot_nl * (1.0 - k) + k);
  float gv = dot_nv / (dot_nv * (1.0 - k) + k);

  return gl * gv;
}

float2 brdf(float no_v, float roughness) {
  float3 n = float3(0, 0, 1);
  float3 v = float3(sqrt(1.0 - no_v * no_v), 0.0, no_v);
  float2 lut = float2(0.0, 0.0);

  for (uint i = 0; i < NUM_SAMPLES; ++i) {
    float2 xi = hammersley_2d(i, NUM_SAMPLES);
    float3 h = importance_sample_ggx(xi, roughness, n);
    float3 l = 2.0 * dot(v, h) * h - v;

    float dot_nl = max(dot(n, l), 0.0);
    float dot_nv = max(dot(n, v), 0.0);
    float dot_vh = max(dot(v, h), 0.0);
    float dot_nh = max(dot(h, n), 0.0);

    if (dot_nl > 0.0) {
      float g = g_schlicksmith_ggx(dot_nl, dot_nv, roughness);
      float g_vis = (g * dot_vh) / (dot_nh * dot_nv);
      float fc = pow(1.0 - dot_vh, 5.0);

      lut += float2((1.0 - fc) * g_vis, fc * g_vis);
    }
  }

  return lut / float(NUM_SAMPLES);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(cs_in input) {
  uint width, height;
  output.GetDimensions(width, height);

  float2 uv = (float2(input.id.xy) + float2(0.5)) / float2(width, height);
  float2 result = brdf(uv.x, 1.0 - uv.y);

  output.Store(input.id.xy, result);
}
