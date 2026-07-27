#ifndef COMMON_SLANG_
#define COMMON_SLANG_

static const float pi = 3.14159265359;
static const float firefly_clamp = 30.0f;

float radical_inverse(uint bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

  return float(bits) * 2.3283064365386963e-10;
}

float2 hammersley(uint i, uint n) { 
  return float2(float(i) / float(n), radical_inverse(i)); 
}

float3 importance_sample_ggx(float2 xi, float3 n, float roughness) {
  float a = roughness * roughness;
  float phi = 2.0 * pi * xi.x;
  float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
  float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

  float3 h = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
  float3 up = abs(n.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  float3 tangent = normalize(cross(up, n));
  float3 bitangent = cross(n, tangent);

  return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}

float geometry_schlick(float ndv, float roughness) {
  float k = (roughness * roughness) / 2.0;

  return ndv / (ndv * (1.0 - k) + k);
}

float3 equirect_direction(float2 uv) {
  float phi = (uv.x * 2.0 - 1.0) * pi;
  float theta = uv.y * pi;
  float st = sin(theta);

  return float3(st * cos(phi), cos(theta), st * sin(phi));
}

float2 direction_to_equirect(float3 d) {
  return float2(atan2(d.z, d.x) / (2.0 * pi) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / pi);
}

#endif // COMMON_SLANG_