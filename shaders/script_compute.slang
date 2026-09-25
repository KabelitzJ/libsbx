// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SCRIPT_COMPUTE_SLANG_
#define SCRIPT_COMPUTE_SLANG_

#include <descriptors.slang>

// Handle types for a script compute shader's push_data struct. Parameters are bound by field
// name via reflection (Sbx.Core.ComputeShader.Set*), and these struct types are how reflection
// tells a sampled texture, a storage texture and a sampler apart from a plain uint. Buffers are
// plain pointers (`my_struct* items;`); their pointee stride is checked against the C# element size.

struct sampled_texture {
  uint index;
}; // struct sampled_texture

struct storage_texture {
  uint index;
}; // struct storage_texture

// Left unset from C#, a sampler_handle field defaults to a clamp-to-edge linear sampler.
struct sampler_handle {
  uint index;
}; // struct sampler_handle

float4 sample_texture(sampled_texture handle, sampler_handle filter, float2 uv) {
  return textures[handle.index].SampleLevel(samplers[filter.index], uv, 0.0);
}

void store_texture(storage_texture handle, uint2 position, float4 value) {
  storage_images[handle.index][position] = value;
}

#endif // SCRIPT_COMPUTE_SLANG_
