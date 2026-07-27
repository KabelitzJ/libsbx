#ifndef DESCRIPTORS_SLANG_
#define DESCRIPTORS_SLANG_

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];

#endif // DESCRIPTORS_SLANG_
