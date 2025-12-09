#ifndef SBX_COMMON_TEXTURE_SAMPLER_SLANG_
#define SBX_COMMON_TEXTURE_SAMPLER_SLANG_

namespace sbx::common {

// struct texture_sampler<T: ITexelElement> {

//   private Texture2D<T> _texture;
//   private SamplerState _sampler;

//   __init(Texture2D<T> texture, SamplerState sampler) {
//     _texture = texture;
//     _sampler = sampler;
//   }

//   T sample(float2 uv) {
//     return _texture.Sample(_sampler, uv);
//   }
// }; // struct texture_sampler

// texture_sampler<float4> create_texture_sampler(uint index) {
//   return texture_sampler<float4>(images[NonUniformResourceIndex(index)], images_sampler);
// }

} // namespace sbx::common

#endif // SBX_COMMON_TEXTURE_SAMPLER_SLANG_
