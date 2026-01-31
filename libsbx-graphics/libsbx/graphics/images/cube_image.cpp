// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/cube_image.hpp>

#include <stb_image.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::graphics {

cube_image::cube_image(const std::filesystem::path& path, const std::string& suffix, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, bool mipmap)
: image{VkExtent3D{0, 0, 1}, filter, address_mode, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VK_FORMAT_R8G8B8A8_SRGB, 1, 6},
  _anisotropic{anisotropic},
  _mipmap{mipmap},
  _channels{4u} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load(assets_module.resolve_path(path), suffix);
}

cube_image::cube_image(const math::vector2u& extent, VkFormat format, VkImageLayout layout, VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode address_mode, VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
: image{VkExtent3D{extent.x(), extent.y(), 1}, filter, address_mode, samples, layout, usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, format, 1, 6},
  _anisotropic{anisotropic},
  _mipmap{mipmap},
  _channels{4u} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load();
}

cube_image::~cube_image() {

}

auto cube_image::_load(const std::filesystem::path& path, const std::string& suffix) -> void {
  _channels = channels_from_format(_format);

  auto* data = static_cast<std::uint8_t*>(nullptr);

  auto buffer = std::vector<std::uint8_t>{};
  auto offset = std::uint32_t{0};

  if (!path.empty()) {
    auto timer = utility::timer{};

    for (const auto& side : side_names) {
      const auto sub_path = path / fmt::format("{}{}", side, suffix);

      if (!std::filesystem::exists(sub_path)) {
        throw std::runtime_error{fmt::format("File does not exist: {}", sub_path.string())};
      }
      
      auto width = std::int32_t{0};
      auto height = std::int32_t{0};
      auto channels = std::int32_t{0};

      stbi_set_flip_vertically_on_load(true);

      auto* image_data = stbi_load(sub_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

      if (!image_data) {
        throw std::runtime_error{fmt::format("Failed to load image: {}", path.string())};
      }

      if (width == 0 || height == 0) {
        throw std::runtime_error{fmt::format("Invalid image size: {} ({}x{})", path.string(), width, height)};
      }

      const auto size = width * height * _channels;

      buffer.resize(buffer.size() + size);
      std::memcpy(buffer.data() + offset, image_data, size);
      offset += size;

      _extent.width = std::max(_extent.width, static_cast<std::uint32_t>(width));
      _extent.height = std::max(_extent.height, static_cast<std::uint32_t>(height));

      stbi_image_free(image_data);
    }

    const auto elapsed = units::quantity_cast<units::millisecond>(timer.elapsed());

    utility::logger<"graphics">::debug("Loaded cube image: {} ({}x{}) in {:.2f}ms", path.string(), _extent.width, _extent.height, elapsed.value());
  }

  if (!buffer.empty()) {
    // throw std::runtime_error{"Failed to load cube image"};
    data = buffer.data();
  }


  if (_extent.width == 0 || _extent.height == 0) {
    return;
  }

  _mip_levels = _mipmap ? mip_levels(_extent) : 1;

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D);
  create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
  create_image_view(_handle, _view, VK_IMAGE_VIEW_TYPE_CUBE, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  create_image_view(_handle, _array_view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  if (data || _mipmap) {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }

  if (data) {
    // [NOTE] KAJ 2023-07-28 : Since we loaded the image with STBI_rgb_alpha, we need to multiply the buffer size by 4.
    auto buffer_size = _extent.width * _extent.height * 4 * _array_layers;
    auto staging_buffer = graphics::staging_buffer{std::span{data, buffer_size}};

    copy_buffer_to_image(staging_buffer, _handle, _extent, _array_layers, 0);
  }

  if (_mipmap) {
    create_mipmaps(_handle, _extent, _format, _layout, _mip_levels, 0, _array_layers);
  } else if (data) {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _layout, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  } else {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, _layout, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }
}

} // namespace sbx::graphics
