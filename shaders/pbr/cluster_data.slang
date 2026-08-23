// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef CLUSTER_DATA_SLANG_
#define CLUSTER_DATA_SLANG_

// Clustered Forward+ grid dimensions. Must match sbx::render::render_module's
// cluster_dim_x/y/z (render_module.hpp) and light_culling_pass's dispatch group math
// (light_culling_pass.cpp) — there's no shared build step between C++ and Slang, so these three
// places are kept in sync by hand, same as every other cross-language struct in shaders/pbr/.
static const uint cluster_dim_x = 16u;
static const uint cluster_dim_y = 9u;
static const uint cluster_dim_z = 24u;

// A cluster's view-space bounding box. Written once per frame by build_clusters.slang, read by
// cull_lights.slang — never touched by the fragment shader, so it isn't part of frame_data.
struct cluster_aabb {
  float4 min_view;
  float4 max_view;
}; // struct cluster_aabb

// One entry per cluster: where its assigned point/spot lights start in the flat light-index list
// and how many there are. Written by cull_lights.slang, read by geometry.slang's fragment shader.
struct cluster_range {
  uint offset;
  uint count;
}; // struct cluster_range

#endif // CLUSTER_DATA_SLANG_
