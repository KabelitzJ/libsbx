// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>
#include <frame_data.slang>

struct vertex_input {
  uint id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

// Mirrors tonemap_pass.cpp's tonemap_push.
struct push_data {
  frame_data* frame;     // fog: view, projection, camera position

  uint color_index;
  uint sampler_index;
  float exposure;
  uint bloom_index;
  float bloom_intensity;

  uint depth_index;
  uint dof_mode;         // 0 off, 1 distance, 2 screen band
  float dof_focus;       // focus distance (world units) or band centre (fraction of height)
  float dof_range;       // focus range or band height
  float dof_max_blur;    // fraction of screen height
  uint dof_samples;
  float near_plane;
  float far_plane;
  float aspect;          // width / height

  uint lut_index;        // 0xFFFFFFFF = none
  float lut_contribution;
  float contrast;
  float saturation;

  float fog_color_r;     // linear; three floats, not a float3 -- that would be 16-byte aligned
  float fog_color_g;
  float fog_color_b;
  float fog_density;     // per world unit at fog_base_height; 0 = off
  float fog_start;       // world units from the camera before fog begins
  float fog_height_falloff;
  float fog_base_height;
  float fog_max_opacity;
  uint fog_affects_sky;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

static const float3x3 input_matrix = float3x3(
  0.59719, 0.35458, 0.04823,
  0.07600, 0.90834, 0.01566,
  0.02840, 0.13383, 0.83777
);

static const float3x3 output_matrix = float3x3(
   1.60475, -0.53108, -0.07367,
  -0.10208,  1.10813, -0.00605,
  -0.00327, -0.07276,  1.07602
);

// Stephen Hill's ACES fit (RRT+ODT) with sRGB<->ACEScg color-space matrices.
float3 tonemap_aces(float3 color) {
  float3 v = mul(input_matrix, color);

  float3 a = v * (v + 0.0245786) - 0.000090537;
  float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;

  return saturate(mul(output_matrix, a / b));
}

// --- depth of field ---------------------------------------------------------------------------

float linear_depth(float2 uv) {
  float d = textures[push.depth_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).r;
  return (push.near_plane * push.far_plane) / (push.far_plane - d * (push.far_plane - push.near_plane));
}

// 0 = sharp, 1 = fully blurred: outside the sharp zone (focus +- range / 2) it ramps up over one
// more range.
float circle_of_confusion(float2 uv) {
  float offset = push.dof_mode == 1u ? abs(linear_depth(uv) - push.dof_focus) : abs(uv.y - push.dof_focus);
  return saturate((offset - push.dof_range * 0.5) / max(push.dof_range, 1e-4));
}

// Gather: dof_samples taps on a golden-angle spiral over this pixel's blur disc. A tap only counts
// as far as its own blur reaches -- a sharp tap near a blurred pixel stays out -- so in-focus
// objects don't smear over the blurred background around them. The centre always counts.
float3 depth_of_field(float2 uv, float3 centre) {
  float coc = circle_of_confusion(uv);

  if (coc <= 0.0) {
    return centre;
  }

  float radius = coc * push.dof_max_blur;  // fraction of the screen height
  float2 scale = float2(radius / push.aspect, radius);
  float3 sum = centre;
  float weight = 1.0;

  for (uint i = 0u; i < push.dof_samples; ++i) {
    float t = (float(i) + 0.5) / float(push.dof_samples);
    float angle = float(i) * 2.39996323;
    float2 offset = float2(cos(angle), sin(angle)) * sqrt(t);
    float2 tap = saturate(uv + offset * scale);  // the sampler repeats: don't wrap across the screen

    float reach = circle_of_confusion(tap) * push.dof_max_blur;
    float w = saturate((reach - sqrt(t) * radius) / max(push.dof_max_blur * 0.1, 1e-5) + 1.0);

    sum += textures[push.color_index].SampleLevel(samplers[push.sampler_index], tap, 0.0).rgb * w;
    weight += w;
  }

  return sum / weight;
}

// --- fog ---------------------------------------------------------------------------------------

// World position of the depth-buffer surface at uv, a linear depth away: the view-space point on
// that pixel's ray, turned back into the world by the view matrix's (rigid) inverse.
float3 world_position(float2 uv, float depth) {
  frame_data frame = *push.frame;
  float2 ndc = uv * 2.0 - 1.0;
  float3 view_position = float3(ndc.x * depth / frame.projection[0][0], ndc.y * depth / frame.projection[1][1], -depth);
  float3x3 rotation = (float3x3)frame.view;

  return frame.camera_position.xyz + mul(transpose(rotation), view_position);
}

// Exponential height fog along the ray from the camera to the surface: density d0 * exp(-f * (h -
// base)) integrated over the part of the ray past fog_start, in closed form.
float fog_amount(float3 camera, float3 surface) {
  float3 ray = surface - camera;
  float distance = length(ray);
  float travel = max(distance - push.fog_start, 0.0);

  if (travel <= 0.0) {
    return 0.0;
  }

  float optical = push.fog_density * travel;

  if (push.fog_height_falloff > 0.0) {
    float start_height = camera.y + ray.y * (1.0 - travel / distance) - push.fog_base_height;
    float k = push.fog_height_falloff * ray.y * (travel / distance);
    float integral = abs(k) > 1e-4 ? (1.0 - exp(-k)) / k : 1.0 - 0.5 * k;
    optical *= exp(-push.fog_height_falloff * start_height) * integral;
  }

  return (1.0 - exp(-optical)) * push.fog_max_opacity;
}

float3 apply_fog(float2 uv, float3 color) {
  float raw = textures[push.depth_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).r;
  bool sky = raw >= 0.99999;

  if (sky && push.fog_affects_sky == 0u) {
    return color;
  }

  float depth = sky ? push.far_plane : linear_depth(uv);
  float3 camera = push.frame->camera_position.xyz;

  float3 fog_color = float3(push.fog_color_r, push.fog_color_g, push.fog_color_b);

  return lerp(color, fog_color, fog_amount(camera, world_position(uv, depth)));
}

// --- colour grading ----------------------------------------------------------------------------

float3 linear_to_srgb(float3 c) {
  return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

float3 srgb_to_linear(float3 c) {
  return select(c <= 0.04045, c / 12.92, pow((c + 0.055) / 1.055, 2.4));
}

// Strip lookup table: size slices of size x size side by side (red along x, green down y), blue
// picks the slice; the two nearest slices are blended.
float3 sample_lut(float3 c) {
  uint width;
  uint height;
  textures[push.lut_index].GetDimensions(width, height);
  float size = float(height);

  float blue = c.b * (size - 1.0);
  float slice = floor(blue);
  float2 inside = float2(c.r * (size - 1.0) + 0.5, c.g * (size - 1.0) + 0.5);
  float2 a = float2((slice * size + inside.x) / (size * size), inside.y / size);
  float2 b = float2((min(slice + 1.0, size - 1.0) * size + inside.x) / (size * size), inside.y / size);

  SamplerState sampler = samplers[push.sampler_index];
  float3 low = textures[push.lut_index].SampleLevel(sampler, a, 0.0).rgb;
  float3 high = textures[push.lut_index].SampleLevel(sampler, b, 0.0).rgb;

  return lerp(low, high, blue - slice);
}

// Contrast and saturation in display space, then the lookup table; neutral settings return c as is.
float3 color_grade(float3 c) {
  if (push.contrast == 1.0 && push.saturation == 1.0 && push.lut_index == 0xFFFFFFFFu) {
    return c;
  }

  float3 display = linear_to_srgb(saturate(c));
  display = (display - 0.5) * push.contrast + 0.5;
  display = lerp(dot(display, float3(0.2126, 0.7152, 0.0722)).xxx, display, push.saturation);
  display = saturate(display);

  if (push.lut_index != 0xFFFFFFFFu) {
    display = lerp(display, sample_lut(display), push.lut_contribution);
  }

  return srgb_to_linear(display);
}

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vertex_output output; 

  output.uv = uv; 
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0); 

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  float3 hdr = textures[push.color_index].Sample(samplers[push.sampler_index], input.uv).rgb;

  if (push.dof_mode != 0u) {
    hdr = depth_of_field(input.uv, hdr);
  }

  if (push.fog_density > 0.0) {
    hdr = apply_fog(input.uv, hdr);
  }

  // Locked to mip 0 (SampleLevel, not Sample) -- bloom_upsample_index is bound against the whole
  // mip chain's default view, and this fullscreen triangle's UV derivatives shouldn't be trusted to
  // pick mip 0 on their own.
  float3 bloom = textures[push.bloom_index].SampleLevel(samplers[push.sampler_index], input.uv, 0.0).rgb;
  hdr += bloom * push.bloom_intensity;

  hdr *= exp2(push.exposure);

  fragment_output output;

  output.color = float4(color_grade(tonemap_aces(hdr)), 1.0);

  return output;
}
