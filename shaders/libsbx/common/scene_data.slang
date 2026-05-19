#ifndef COMMON_SCENE_DATA_SLANG_
#define COMMON_SCENE_DATA_SLANG_

namespace sbx::common {

struct scene_data {
  float4x4 view;
  float4x4 projection;
  float4x4 inverse_view;
  float4x4 inverse_projection;

  float3 camera_position;
  float camera_near;

  float3 camera_up;
  float camera_far;

  float3 camera_right;
  float camera_fov_radians;

  float3 light_direction;
  float time;

  float4 light_color;
  float4x4 light_spaces[4];
  float4 cascade_splits;

  uint2 viewport;
  float2 wind_direction;

  float wind_strength;
  float3 _pad;
}; // struct scene_data

} // namespace sbx::common

#endif // COMMON_SCENE_DATA_SLANG_
