#include <libsbx/graphics/images/image2d.hpp>

#include <stb_image.h>

#include <fmt/format.h>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>

namespace sbx::graphics {

image2d::image2d(const math::vector2u& extent, graphics::format format, VkImageLayout layout, VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode address_mode, VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
: image{VkExtent3D{extent.x(), extent.y(), 1}, filter, address_mode, samples, layout, (usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), to_vk_enum<VkFormat>(format), 1, 1},
  _anisotropic{anisotropic},
  _mipmap{mipmap} {
  _load();
}

// [TODO] KAJ 2023-07-28 : We use VK_FORMAT_R8G8B8A8_SRGB here because it best matches the STBI_rgb_alpha format that we load the image with.
image2d::image2d(const std::filesystem::path& path, graphics::format format, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, bool mipmap)
: image{VkExtent3D{0, 0, 1}, filter, address_mode, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), to_vk_enum<VkFormat>(format), 1, 1},
  _anisotropic{anisotropic},
  _mipmap{mipmap} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load(assets_module.resolve_path(path));
}

image2d::image2d(const math::vector2u& extent, graphics::format format, memory::observer_ptr<const std::uint8_t> pixels)
: image2d{extent, format} {
  set_pixels(pixels);
}

static auto bytes_per_channel(VkFormat format) -> std::uint8_t {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: {
      return 1;
    }
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT: {
      return 2;
    }
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT: {
      return 4;
    }
    default: {
      throw std::runtime_error{fmt::format("Unsupported image format: {}", static_cast<std::int32_t>(format))};
    }
  }
}

auto image2d::set_pixels(memory::observer_ptr<const std::uint8_t> pixels) -> void {
  auto buffer_size = _extent.width * _extent.height * _channels * bytes_per_channel(_format);
  auto staging_buffer = graphics::staging_buffer{std::span{pixels.get(), buffer_size}};

  transition_image_layout(_handle, _format, _layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  copy_buffer_to_image(staging_buffer, _handle, _extent, _array_layers, 0);

  if (_mipmap) {
    create_mipmaps(_handle, _extent, _format, _layout, _mip_levels, 0, _array_layers);
  } else {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _layout, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }
}

struct file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t channels;
}; // struct file_header

struct image_data {
  file_header header;
  std::uint8_t* pixels;
}; // struct image_data

auto image2d::_load(const std::filesystem::path& path) -> void {
  // [TODO] KAJ 2025-05-26 : This code is absolutely terrible, it should be refactored to use a more robust image loading system.
  // const auto needs_processing = !std::filesystem::exists(std::filesystem::path{path}.replace_extension(".sbximg"));

  _channels = channels_from_format(_format);

  auto data = image_data{};

  if (!path.empty()) {
    auto timer = utility::timer{};

    stbi_set_flip_vertically_on_load(true);

    // [NOTE] KAJ 2023-07-28 : Force 4 channels (RGBA) and ignore the original image's channels.
    data.pixels = stbi_load(path.string().c_str(), reinterpret_cast<std::int32_t*>(&_extent.width), reinterpret_cast<std::int32_t*>(&_extent.height), nullptr, STBI_rgb_alpha);

    if (!data.pixels) {
      throw std::runtime_error{fmt::format("Failed to load image: {}", path.string())};
    }

    if (_extent.width == 0 || _extent.height == 0) {
      throw std::runtime_error{fmt::format("Image '{}' has invalid dimensions: {}x{}", path.string(), _extent.width, _extent.height)};
    }

    const auto elapsed = units::quantity_cast<units::millisecond>(timer.elapsed());
  
    utility::logger<"graphics">::debug("Loaded image: {} ({}x{}) in {:.2f}ms", path.string(), _extent.width, _extent.height, elapsed.value());
  }


  _mip_levels = _mipmap ? mip_levels(_extent) : 1;

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D);
  create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
  create_image_view(_handle, _view, VK_IMAGE_VIEW_TYPE_2D, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  if (data.pixels || _mipmap) {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }

  if (data.pixels) {
    // [NOTE] KAJ 2023-07-28 : Since we loaded the image with STBI_rgb_alpha, we need to multiply the buffer size by 4.
    const auto buffer_size = _extent.width * _extent.height * 4u;
    auto staging_buffer = graphics::staging_buffer{std::span{data.pixels, buffer_size}};

    copy_buffer_to_image(staging_buffer, _handle, _extent, _array_layers, 0);

    stbi_image_free(data.pixels);
  }

  if (_mipmap) {
    create_mipmaps(_handle, _extent, _format, _layout, _mip_levels, 0, _array_layers);
  } else if (data.pixels) {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _layout, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  } else {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, _layout, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }
}

} // namespace sbx::graphics

