// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef FRAME_DATA_SLANG_
#define FRAME_DATA_SLANG_

#include <common.slang>
#include <descriptors.slang>

static const uint shadow_cascade_count = 4;

static const uint material_max_params = 8;
static const uint material_max_textures = 16;

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
  float4 color;
}; // struct vertex

// Mirrors render_packet.hpp's transform_data. color multiplies the vertex colour in the engine's
// shaders (white unless instanced); custom_data is a material's own per-instance data.
struct transform_data {
  float4x4 model;
  float4x4 normal;
  float4 color;
  float4 custom_data;
}; // struct transform_data

// An instance's LOD crossfade. frustum_cull keeps it in a culled transform's normal[3][3], which the normal transform never reads (w = 0); a source transform has 1 there.
float lod_fade_of(transform_data instance) {
  return instance.normal[3][3];
}

// Screen-door dither for a level crossfading with its neighbour: 1 = drawn whole; [0, 1) = the level fading in, kept where the dither is below it;
// (1, 2] = the level fading out, kept where the dither reaches fade - 1. The two halves of one fade cover every pixel exactly once, in every pass.
void apply_lod_fade(float fade, float4 sv_position) {
  if (fade == 1.0) {
    return;
  }

  float noise = frac(52.9829189 * frac(dot(sv_position.xy, float2(0.06711056, 0.00583715))));
  bool keep = fade > 1.0 ? noise >= fade - 1.0 : noise < fade;

  if (!keep) {
    discard;
  }
}

enum class material_flags : uint {
  none = 0,
  alpha_masked = 1 << 0,
  receives_shadow = 1 << 1
}; // enum material_flags

struct material_data {
  float4 base_color_factor;
  float4 emissive_factor;
  uint albedo_index;
  uint normal_index;
  uint metallic_roughness_index;
  uint occlusion_index;
  uint emissive_index;
  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
  float normal_scale;
  float occlusion_strength;
  float emissive_strength;
  float ior;
  float2 uv_tiling;
  float2 uv_offset;
  float4 generic_params[material_max_params];
  uint generic_textures[material_max_textures];
}; // struct material_data

float2 apply_uv_transform(material_data material, float2 uv) {
  return uv * material.uv_tiling + material.uv_offset;
}

void apply_alpha_cutout(material_data material, float2 uv, uint sampler_index) {
  float2 transformed_uv = apply_uv_transform(material, uv);
  float alpha = material.base_color_factor.a * textures[material.albedo_index].Sample(samplers[sampler_index], transformed_uv).a;
  uint is_alpha_masked = (material.flags & material_flags::alpha_masked) != 0u;

  clip(is_alpha_masked * (alpha - material.alpha_cutoff));
}

enum class light_type : uint {
  directional = 0,
  point = 1,
  spot = 2
}; // enum light_type

struct light_data {
  float4 color;
  float4 position;
  float4 direction;
  light_type type;
  float inner_cos;
  float outer_cos;
  uint shadow_index; // into frame_data.local_shadow_views, 0xFFFFFFFF: no shadow
}; // struct light_data

// One local light shadow tile; mirrors render_packet.hpp's local_shadow_view.
struct local_shadow_view {
  float4x4 view_projection;
  float4 atlas_rect; // xy: top-left, zw: size, both in atlas uv
  float4 bias;       // x: depth bias, y: normal bias (texels), z: one texel's world size at unit distance
}; // struct local_shadow_view

// One projected decal; mirrors render_packet.hpp's decal_data.
// The decal is the unit cube [-0.5, 0.5]^3 in its own space, projecting along its -Z onto whatever lies inside it.
struct decal_data {
  float4x4 world_to_decal;
  float4 center;  // xyz: world-space box center
  float4 axis_x;  // xyz: world-space half extent along the decal's X; w: normal strength
  float4 axis_y;  // xyz: the same along Y; w: cosine of the angle (surface normal vs +Z) where the angle fade starts
  float4 axis_z;  // xyz: the same along Z, pointing back at the projector; w: cosine of the angle where the decal is gone
  float4 color;   // rgb: albedo tint, a: opacity
  uint albedo_index; // bindless texture, 0xFFFFFFFF: the tint alone
  uint normal_index; // bindless tangent-space normal map, 0xFFFFFFFF: none
  float edge_fade;   // fraction of the box depth faded out at its near and far faces
  uint padding;
}; // struct decal_data

// One baked reflection probe; mirrors render_packet.hpp's probe_data.
// Its box is centered on where it was captured and turns with its node, but ignores the node's scale.
struct probe_data {
  float4x4 world_to_box; // world -> box space: rotation and translation only, so lengths stay in world units
  float4 center;         // xyz: world-space box center (also the capture point), w: blend distance
  float4 axis_x;         // xyz: world-space half extent along the box's X, w: intensity
  float4 axis_y;         // xyz: the same along Y, w: 1 for box projection, 0 for an infinitely distant capture
  float4 axis_z;         // xyz: the same along Z
  float4 extents;        // xyz: half extents in box space
  uint irradiance_index; // bindless cubes
  uint prefiltered_index;
  uint prefiltered_mip_count;
  uint padding;
}; // struct probe_data

// One cluster's slice of frame_data.cluster_indices: its lights, then its decals, then its probes.
struct cluster_range {
  uint offset;
  uint light_count;
  uint decal_count;
  uint probe_count;
}; // struct cluster_range

struct frame_data {
  float4x4 view;
  float4x4 projection;
  float4 camera_position;
  light_data* lights;
  uint light_count;
  uint padding;
  material_data* materials;
  uint irradiance_index;
  uint brdf_lut_index;
  uint prefiltered_index;
  uint prefiltered_mip_count;
  float environment_intensity;
  float ambient_intensity;
  uint directional_light_count;
  float cluster_scale;
  cluster_range* cluster_ranges;
  uint* cluster_indices;
  float cluster_bias;
  float2 cluster_tile_size;
  float4 cascade_splits;
  float4 cascade_depth_bias_per_texel;
  float4 cascade_texel_world_size;
  float4 cascade_spheres[shadow_cascade_count]; // xyz = world center, w = radius
  float4x4 light_view_projections[shadow_cascade_count];
  uint shadow_map_indices[shadow_cascade_count];
  uint shadow_enabled;
  float4 frustum_planes[6];
  uint scene_depth_index;
  float near_plane;
  float far_plane;
  float2 render_target_size;
  uint shadow_sampler_index;
  uint shadow_cascade_debug;
  float shadow_depth_bias;
  float shadow_normal_bias;
  float shadow_penumbra_scale; // tan(half the light's angular diameter): PCSS penumbra radius per unit of blocker distance
  float contact_shadow_length;
  uint ambient_occlusion_index;  // ambient_occlusion_pass's result, 0xFFFFFFFF: off
  uint local_shadow_view_count;
  local_shadow_view* local_shadow_views; // local light shadow tiles: shadow views shadow_cascade_count and up
  uint local_shadow_atlas_index;
  uint local_shadow_padding;
  float lod_projection_scale;       // pixels covered by one world unit at distance 1 from the camera
  float lod_error_threshold;        // pixels of simplification error a level may show, camera view
  float shadow_lod_error_threshold; // the same for shadow views
  uint lod_padding;
  uint* occlusion_visibility;      // per camera-view instance: 1 if it was visible last frame (see frustum_cull's occlusion modes)
  uint* occlusion_culled_counter;  // instances the late cull found hidden behind the depth pyramid this frame
  uint hiz_index;                  // the depth pyramid, sampled through Load with an explicit mip
  uint hiz_width;
  uint hiz_height;
  uint hiz_mip_count;
  uint occlusion_enabled;
  uint occlusion_padding;
  decal_data* decals;              // sorted by sort order: later ones draw over earlier ones
  uint decal_count;
  uint decal_padding;
  probe_data* probes;              // most important first
  uint probe_count;
  uint probe_padding;
}; // struct frame_data

// The view-projection of shadow view `view`: 0 to shadow_cascade_count - 1 are the sun's cascades, the rest local light shadow tiles.
// A depth vertex shader's push.cascade_index is a shadow view index; use this rather than indexing light_view_projections, which only holds the cascades.
float4x4 shadow_view_projection(frame_data frame, uint view) {
  return view < shadow_cascade_count ? frame.light_view_projections[view] : frame.local_shadow_views[view - shadow_cascade_count].view_projection;
}

#endif // FRAME_DATA_SLANG_
