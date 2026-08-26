// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef COMMON_SLANG_
#define COMMON_SLANG_

#include <math.slang>

struct range {
  uint offset;
  uint count;
}; // struct range

struct aabb {
  float4 min;
  float4 max;
}; // struct aabb

struct dispatch_indirect_command {
  uint group_count_x;
  uint group_count_y;
  uint group_count_z;
}; // struct dispatch_indirect_command

struct draw_indirect_command {
  uint vertex_count;
  uint instance_count;
  uint first_vertex;
  uint first_instance;
}; // struct draw_indirect_command

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
  float phi = two_pi * xi.x;
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

/**
 * @brief The direction for a given (uv, face) texel of a cube image, following the standard
 * Vulkan/D3D cube array-layer order: +X, -X, +Y, -Y, +Z, -Z.
 */
float3 cube_direction(float2 uv, uint face) {
  float2 st = uv * 2.0 - 1.0;

  switch (face) {
    case 0: return float3( 1.0, -st.y, -st.x);
    case 1: return float3(-1.0, -st.y,  st.x);
    case 2: return float3( st.x,  1.0,  st.y);
    case 3: return float3( st.x, -1.0, -st.y);
    case 4: return float3( st.x, -st.y,  1.0);
    default: return float3(-st.x, -st.y, -1.0);
  }
}

#endif // COMMON_SLANG_
