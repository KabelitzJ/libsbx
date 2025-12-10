#include <libsbx/common/scene_data.slang>
#include <libsbx/common/depth.slang>
#include <libsbx/math/constants.slang>
#include <libsbx/math/algorithm.slang>

struct fs_in {
  float2 uv : TEXCOORD0;
};

struct fs_out {
  float occlusion : SV_Target0;
};

struct push_data {
  uint kernel_size;
  float kernel_radius;
  float3* kernel_buffer;
  float bias;
};

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
Sampler2D<float4> position_image;

[[vk::binding(2, 0)]]
Sampler2D<float4> normal_image;

[[vk::binding(3, 0)]]
Sampler2D<float4> noise_image;

float3x3 build_tbn(float3 normal) {
  float3 up = (abs(normal.z) < 0.999f) ? float3(0,0,1) : float3(0,1,0);
  float3 tangent = normalize(cross(up, normal));
  float3 bitangent = cross(normal, tangent);
  return float3x3(tangent, bitangent, normal);
}

[shader("fragment")]
fs_out main(fs_in in) {
  fs_out out;

  float3 normal_world = normalize(normal_image.Sample(in.uv).xyz * 2.0f - 1.0f);
  float3 normal_view = normalize(mul(scene.view, float4(normal_world, 0.0f)).xyz);

  float3 position_world = position_image.Sample(in.uv).xyz;
  float3 position_view = mul(scene.view, float4(position_world, 1.0f)).xyz;

  float depth_view = -position_view.z;

  float3x3 TBN = build_tbn(normal_view);

  int2 tex_dim;
  position_image.GetDimensions(tex_dim.x, tex_dim.y);

  int2 noise_dim;
  noise_image.GetDimensions(noise_dim.x, noise_dim.y);

  float2 noise_uv = float2(float(tex_dim.x) / float(noise_dim.x), float(tex_dim.y) / (noise_dim.y)) * in.uv;
  float3 random = noise_image.Sample(noise_uv).xyz * 2.0 - 1.0;

  float3 tangent = normalize(random - normal_view * dot(random, normal_view));
  float3 bitangent = cross(normal_view, tangent);
  float3x3 basis = float3x3(tangent, bitangent, normal_view);

  float occlusion = 0.0;
  uint count = push.kernel_size;

  [loop]
  for (uint i = 0u; i < count; ++i) {
    float3 sample_vec = mul(basis, push.kernel_buffer[i]);
    float3 sample_pos = position_view + sample_vec * push.kernel_radius;

    float4 sample_proj = mul(scene.projection, float4(sample_pos, 1.0f));
    sample_proj.xyz /= sample_proj.w;

    float2 sample_uv = sample_proj.xy * 0.5f + 0.5f;

    if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
      continue;
    }

    float3 fetched_world = position_image.Sample(sample_uv).xyz;
    float3 fetched_view = mul(scene.view, float4(fetched_world, 1.0f)).xyz;
    float fetched_depth = -fetched_view.z;

    float range_check = smoothstep(0.0, 1.0, push.kernel_radius / abs(depth_view - fetched_depth));

    occlusion += (fetched_depth >= sample_pos.z + push.bias ? 1.0 : 0.0) * range_check;
  }

  out.occlusion = occlusion / count;

  return out;
}
