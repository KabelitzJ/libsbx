#ifndef VEHICLE_DIRT_SLANG_
#define VEHICLE_DIRT_SLANG_

#include <libsbx/common/image.slang>

#include <deferred_pbr_material/surface_context.slang>

void sbx_surface_vertex(inout surface_vertex_context ctx) { }

void sbx_surface_fragment(inout surface_fragment_context ctx) {
  ctx.albedo = float4(1, 0, 1, 1);
  ctx.emissive = float3(0, 0, 0);
}

#endif
