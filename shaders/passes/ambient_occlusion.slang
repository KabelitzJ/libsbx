// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>
#include <frame_data.slang>

// Screen-space ambient occlusion (ambient_occlusion_pass.hpp), at half resolution:
//   occlusion_main: per pixel, the view-space position and normal from the full-resolution depth,
//                   then `samples` points in the normal's hemisphere (golden-angle spiral, rotated
//                   per pixel by interleaved-gradient noise), each counted as occluded when the
//                   depth buffer shows a surface in front of it -- weighted by a range check, so
//                   far-away geometry in front doesn't darken. Writes 1 - intensity * occlusion.
//   blur_main:      a 5x5 blur of that, weighted by depth similarity so it doesn't cross edges.

// Mirrors ambient_occlusion_pass.cpp's ambient_occlusion_push.
struct push_data {
  frame_data* frame;
  uint depth_index;
  uint input_index;   // blur: the raw result (sampled index)
  uint output_index;  // storage index
  float radius;       // world units
  float intensity;
  uint samples;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

static const float sky_depth = 0.99999;

float raw_depth(int2 pixel) {
  return textures[push.depth_index].Load(int3(pixel, 0)).r;
}

float linearize(float raw) {
  frame_data frame = *push.frame;
  return (frame.near_plane * frame.far_plane) / (frame.far_plane - raw * (frame.far_plane - frame.near_plane));
}

// View-space position (camera looks down -z) of the depth-buffer surface at a full-res pixel.
float3 view_position(int2 pixel, float2 size) {
  frame_data frame = *push.frame;
  float d = linearize(raw_depth(pixel));
  float2 ndc = (float2(pixel) + 0.5) / size * 2.0 - 1.0;

  return float3(ndc.x * d / frame.projection[0][0], ndc.y * d / frame.projection[1][1], -d);
}

// Full-res pixel a view-space point projects to.
float2 project(float3 p, float2 size) {
  frame_data frame = *push.frame;
  float2 ndc = float2(p.x * frame.projection[0][0], p.y * frame.projection[1][1]) / -p.z;

  return (ndc * 0.5 + 0.5) * size;
}

// Jorge Jimenez's interleaved gradient noise: a per-pixel rotation that blurs out evenly.
float interleaved_gradient_noise(float2 pixel) {
  return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void occlusion_main(compute_input input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint depth_width, depth_height;
  textures[push.depth_index].GetDimensions(depth_width, depth_height);
  float2 size = float2(depth_width, depth_height);
  int2 pixel = min(int2(input.id.xy) * 2, int2(size) - 1);

  if (raw_depth(pixel) >= sky_depth) {
    storage_images[push.output_index][input.id.xy] = float4(1.0, 1.0, 1.0, 1.0);
    return;
  }

  // Position, and the normal from the neighbours on whichever side is closer in depth (the other
  // may lie across an edge).
  float3 p = view_position(pixel, size);
  float3 right = view_position(min(pixel + int2(1, 0), int2(size) - 1), size) - p;
  float3 left = p - view_position(max(pixel - int2(1, 0), int2(0, 0)), size);
  float3 down = view_position(min(pixel + int2(0, 1), int2(size) - 1), size) - p;
  float3 up = p - view_position(max(pixel - int2(0, 1), int2(0, 0)), size);
  float3 dx = abs(right.z) < abs(left.z) ? right : left;
  float3 dy = abs(down.z) < abs(up.z) ? down : up;
  float3 n = normalize(cross(dx, dy));

  if (dot(n, -p) < 0.0) {
    n = -n;
  }

  float3 tangent = normalize(cross(n, abs(n.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0)));
  float3 bitangent = cross(n, tangent);

  float rotation = interleaved_gradient_noise(float2(input.id.xy)) * 6.28318531;
  float bias = push.radius * 0.02;
  float occlusion = 0.0;

  for (uint i = 0u; i < push.samples; ++i) {
    float t = (float(i) + 0.5) / float(push.samples);
    float phi = float(i) * 2.39996323 + rotation;
    float cos_theta = 1.0 - t;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    float3 direction = (tangent * cos(phi) + bitangent * sin(phi)) * sin_theta + n * cos_theta;

    // More samples close in: lerp(0.1, 1, t^2) of the radius.
    float3 q = p + direction * push.radius * lerp(0.1, 1.0, t * t);
    float2 at = project(q, size);

    if (any(at < 0.0) || any(at >= size)) {
      continue;
    }

    float scene = linearize(raw_depth(int2(at)));
    float blocked = scene < -q.z - bias ? 1.0 : 0.0;
    float in_range = smoothstep(0.0, 1.0, push.radius / max(abs(-p.z - scene), 1e-4));
    occlusion += blocked * in_range;
  }

  float ao = saturate(1.0 - push.intensity * occlusion / float(push.samples));
  storage_images[push.output_index][input.id.xy] = float4(ao, ao, ao, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void blur_main(compute_input input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint depth_width, depth_height;
  textures[push.depth_index].GetDimensions(depth_width, depth_height);
  int2 depth_max = int2(depth_width, depth_height) - 1;
  int2 max_pixel = int2(width, height) - 1;

  float centre_depth = linearize(raw_depth(min(int2(input.id.xy) * 2, depth_max)));
  float sum = 0.0;
  float weight = 0.0;

  for (int y = -2; y <= 2; ++y) {
    for (int x = -2; x <= 2; ++x) {
      int2 at = clamp(int2(input.id.xy) + int2(x, y), int2(0, 0), max_pixel);
      float depth = linearize(raw_depth(min(at * 2, depth_max)));

      // Same surface (within 5% of the depth): full weight, fading out beyond.
      float w = saturate(1.0 - abs(depth - centre_depth) / (0.05 * centre_depth + 1e-4));
      sum += textures[push.input_index].Load(int3(at, 0)).r * w;
      weight += w;
    }
  }

  float ao = weight > 0.0 ? sum / weight : 1.0;
  storage_images[push.output_index][input.id.xy] = float4(ao, ao, ao, 1.0);
}
