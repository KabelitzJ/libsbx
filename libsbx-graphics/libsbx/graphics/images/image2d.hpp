#ifndef LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_
#define LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_

#include <filesystem>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/resource_storage.hpp>

#include <libsbx/graphics/images/image.hpp>

namespace sbx::graphics {

enum class format : std::int32_t {
  undefined = VK_FORMAT_UNDEFINED,
  r8_unorm = VK_FORMAT_R8_UNORM,
  r16_sfloat = VK_FORMAT_R16_SFLOAT,
  r32_sfloat = VK_FORMAT_R32_SFLOAT,
  r32_uint = VK_FORMAT_R32_UINT,
  r64_uint = VK_FORMAT_R64_UINT,
  r16g16_sfloat = VK_FORMAT_R16G16_SFLOAT,
  r32g32_sfloat = VK_FORMAT_R32G32_SFLOAT,
  r32g32_uint = VK_FORMAT_R32G32_UINT,
  r8g8b8a8_unorm = VK_FORMAT_R8G8B8A8_UNORM,
  r8g8b8a8_srgb = VK_FORMAT_R8G8B8A8_SRGB,
  b8g8r8a8_srgb = VK_FORMAT_B8G8R8A8_SRGB,
  a2b10g10r10_unorm_pack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
  r16g16b16a16_sfloat = VK_FORMAT_R16G16B16A16_SFLOAT,
  r32g32b32a32_sfloat = VK_FORMAT_R32G32B32A32_SFLOAT
}; // enum class format

enum class address_mode : std::uint8_t {
  repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  mirror = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
  clamp_to_edge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  clamp_to_border = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
}; // enum class address_mode

enum class filter : std::uint8_t {
  nearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
  linear = VK_SAMPLER_MIPMAP_MODE_LINEAR
}; // enum class filter

class image2d : public image {

public:

  image2d(const math::vector2u& extent, graphics::format format = graphics::format::r8g8b8a8_unorm, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

  image2d(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT, bool anisotropic = false, bool mipmap = false);

  image2d(const math::vector2u& extent, graphics::format format, memory::observer_ptr<const std::uint8_t> pixels);

  ~image2d() override = default;

  auto set_pixels(memory::observer_ptr<const std::uint8_t> pixels) -> void;

  auto name() const noexcept -> std::string override {
    return "Image 2D";
  }

private:

  auto _load(const std::filesystem::path& path = {}) -> void;

  bool _anisotropic;
	bool _mipmap;
  std::uint8_t _channels;

}; // class image2d

using image2d_handle = resource_handle<image2d>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_
