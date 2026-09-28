// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADOWS_CONTACT_SLANG_
#define SHADOWS_CONTACT_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>

static const uint contact_shadow_step_count = 12u;

float contact_shadow_linear_depth(frame_data frame, float depth) {
  return (frame.near_plane * frame.far_plane) / (frame.far_plane - depth * (frame.far_plane - frame.near_plane));
}

// Screen-space ray march from world_position toward the light through the depth pre-pass result.
// Catches the small-scale occlusion the cascades lose to texel size and bias. A depth-buffer hit
// only counts while the ray is at most `thickness` behind it, since the buffer stores front faces
// only -- without that, every thin object would shadow everything behind it.
//
// depth_slope is fwidth() of the shaded surface's linear depth, i.e. how much its own depth changes
// per pixel. The depth buffer can't be compared against more precisely than that: a min depth
// resolve returns the closest of the pixel's MSAA samples (up to ~0.4 px off-center), and each ray
// step reads the nearest pixel center (up to 0.5 px off). On a surface seen at a grazing angle one
// pixel spans centimeters of depth, so without this tolerance the ray "hits" the surface it starts on.
float calculate_contact_shadow(frame_data frame, float3 world_position, float3 light_direction, float jitter, float depth_slope) {
  float ray_length = frame.contact_shadow_length;
  float step_length = ray_length / float(contact_shadow_step_count);
  float thickness = ray_length * 0.5;
  float tolerance = step_length * 0.1 + depth_slope;

  float3 view_start = mul(frame.view, float4(world_position, 1.0)).xyz;
  float3 view_direction = mul(frame.view, float4(light_direction, 0.0)).xyz;

  int2 max_pixel = int2(frame.render_target_size) - 1;

  for (uint i = 0u; i < contact_shadow_step_count; ++i) {
    float3 position = view_start + view_direction * ((float(i) + jitter) * step_length);
    float4 clip = mul(frame.projection, float4(position, 1.0));

    if (clip.w <= 0.0) {
      break;
    }

    float2 uv = clip.xy / clip.w * 0.5 + 0.5;

    if (any(uv < 0.0) || any(uv > 1.0)) {
      break;
    }

    int2 pixel = clamp(int2(uv * frame.render_target_size), int2(0, 0), max_pixel);
    float scene_depth = contact_shadow_linear_depth(frame, textures[frame.scene_depth_index].Load(int3(pixel, 0)).r);

    float delta = -position.z - scene_depth;

    if (delta > tolerance && delta < thickness) {
      return 0.0;
    }
  }

  return 1.0;
}

#endif // SHADOWS_CONTACT_SLANG_
