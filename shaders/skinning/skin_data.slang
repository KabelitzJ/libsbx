// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SKIN_DATA_SLANG_
#define SKIN_DATA_SLANG_

// Mirrors sbx::assets::skin_vertex (libsbx/assets/mesh.hpp) byte-for-byte -- a separate array
// indexed 1:1 with frame_data.slang's vertex, not merged into it.
struct skin_vertex {
  uint4 joint_indices;
  float4 weights;
}; // struct skin_vertex

#endif // SKIN_DATA_SLANG_
