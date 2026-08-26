// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef DESCRIPTORS_SLANG_
#define DESCRIPTORS_SLANG_

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];
[[vk::binding(2, 0)]] RWTexture2D<float4> storage_images[];
[[vk::binding(3, 0)]] TextureCube cubes[];
[[vk::binding(4, 0)]] RWTexture2DArray<float4> storage_cubes[];

#endif // DESCRIPTORS_SLANG_
