#ifndef MATERIAL_TEXTURE_SLOT_SLANG_
#define MATERIAL_TEXTURE_SLOT_SLANG_

#include <libsbx/common/image.slang>

struct texture_slot<T: ITexelElement, let ImageCount: int = sbx::common::max_image_array_size, let SamplerCount: int = sbx::common::max_sampler_array_size> {
  uint image_index;
  uint sampler_index;

  bool is_valid() {
    return image_index < ImageCount;
  }

  T sample(Texture2D<T> images[ImageCount], SamplerState samplers[SamplerCount], float2 uv) {
    Texture2D<T> image = images[NonUniformResourceIndex(image_index)];
    
    return image.Sample(samplers[NonUniformResourceIndex(sampler_index)], uv);
  }

  T sample_or(Texture2D<T> images[ImageCount], SamplerState samplers[SamplerCount], float2 uv, T fallback) {
    return is_valid() ? sample(images, samplers, uv) : fallback;
  }

}; // struct texture_slot

#endif // MATERIAL_TEXTURE_SLOT_SLANG_