#ifndef SURFACE_CONTEXT_SLANG_
#define SURFACE_CONTEXT_SLANG_

#include <deferred_pbr_material/common.slang>

#ifndef SBX_SURFACE_INCLUDE
  #define SBX_SURFACE_INCLUDE "deferred_pbr_material/default_surface.slang"
#endif

struct surface_vertex_context {
  uint vertex_id;
  uint instance_id;
  float3 local_position;
  float3 local_normal;
  float4 local_tangent;
  float2 local_uv;
  material_data material;

  // writable
  float3 world_position;
  float3 world_normal;
  float4 world_tangent;
  float2 uv;
};

struct surface_fragment_context {
  float3 position;
  float3 normal;
  float4 tangent;
  float2 uv;
  nointerpolation uint instance_index;
  material_data material;

#if defined(VERTEX_STREAM_COLOR)
  float4 color;
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  float4 custom0;
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  float4 custom1;
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  float4 custom2;
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  float4 custom3;
#endif

  // writable
  float4 albedo;
  float3 world_normal;
  float3 metallic_roughness_occlusion;
  float3 emissive;
}; // struct surface_fragment_context

#endif // SURFACE_CONTEXT_SLANG_
