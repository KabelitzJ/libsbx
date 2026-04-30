// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/cube_image.hpp>

#include <stb_image.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

cube_image::cube_image(const std::filesystem::path& path, const std::string& suffix, graphics::format format, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, bool mipmap)
: image{VkExtent3D{0, 0, 1}, filter, address_mode, VK_SAMPLE_COUNT_1_BIT, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), to_vk_enum<VkFormat>(format), 1, 6},
  _anisotropic{anisotropic},
  _mipmap{mipmap},
  _channels{4u} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load(assets_module.resolve_path(path), suffix);
}

cube_image::cube_image(const math::vector2u& extent, VkFormat format, VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode address_mode, VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
: image{VkExtent3D{extent.x(), extent.y(), 1}, filter, address_mode, samples, usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, format, 1, 6},
  _anisotropic{anisotropic},
  _mipmap{mipmap},
  _channels{4u} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load();
}

cube_image::~cube_image() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  vkDestroyImageView(logical_device, _array_view, nullptr);
}

auto bytes_per_pixel(const VkFormat format) -> std::uint32_t {
  switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: {
      return 4u;
    }
    case VK_FORMAT_R16G16B16A16_SFLOAT: {
      return 8u;
    }
    case VK_FORMAT_R32G32B32A32_SFLOAT: {
      return 16u;
    }
    default: {
      throw std::runtime_error{fmt::format("Unsupported cube image format: {}", static_cast<std::int32_t>(format))};
    }
  }
}

auto cube_image::_load(const std::filesystem::path& path, const std::string& suffix) -> void {
  SBX_PROFILE_SCOPE("cube_image::_load");

  _channels = channels_from_format(_format);

  const auto bpp = bytes_per_pixel(_format);
  const auto is_hdr_format = (_format == VK_FORMAT_R16G16B16A16_SFLOAT || _format == VK_FORMAT_R32G32B32A32_SFLOAT);

  auto buffer = std::vector<std::uint8_t>{};
  auto offset = std::uint32_t{0};

  auto from_file = false;

  if (!path.empty()) {
    SBX_PROFILE_SCOPE("cube_image::_load_from_file");

    auto timer = utility::timer{};

    from_file = true;

    const auto first_path = path / fmt::format("{}{}", side_names[0], suffix);
    const auto is_hdr_file = stbi_is_hdr(first_path.string().c_str());

    utility::assert_that(is_hdr_file == is_hdr_format, "Cube image HDR-ness does not match target format");
    utility::assert_that(!is_hdr_format || _format == VK_FORMAT_R32G32B32A32_SFLOAT, "HDR cube image currently requires R32G32B32A32_SFLOAT (R16 target needs CPU-side f32->f16 conversion)");

    stbi_set_flip_vertically_on_load(true);

    for (const auto& side : side_names) {
      const auto sub_path = path / fmt::format("{}{}", side, suffix);

      if (!std::filesystem::exists(sub_path)) {
        throw std::runtime_error{fmt::format("File does not exist: {}", sub_path.string())};
      }

      auto width = std::int32_t{0};
      auto height = std::int32_t{0};
      auto channels = std::int32_t{0};

      if (is_hdr_file) {
        utility::assert_that(stbi_is_hdr(sub_path.string().c_str()), "Cube image was determined to be HDR but face is not");

        stbi_set_flip_vertically_on_load(false);

        auto* image_data = stbi_loadf(sub_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!image_data) {
          throw std::runtime_error{fmt::format("Failed to load HDR image: {}", sub_path.string())};
        }

        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp;

        buffer.resize(buffer.size() + size);
        std::memcpy(buffer.data() + offset, image_data, size);
        offset += static_cast<std::uint32_t>(size);

        stbi_image_free(image_data);
      } else {
        stbi_set_flip_vertically_on_load(true);

        auto* image_data = stbi_load(sub_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!image_data) {
          throw std::runtime_error{fmt::format("Failed to load image: {}", sub_path.string())};
        }

        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp;

        buffer.resize(buffer.size() + size);
        std::memcpy(buffer.data() + offset, image_data, size);
        offset += static_cast<std::uint32_t>(size);

        stbi_image_free(image_data);
      }

      _extent.width = std::max(_extent.width, static_cast<std::uint32_t>(width));
      _extent.height = std::max(_extent.height, static_cast<std::uint32_t>(height));
    }

    const auto elapsed = units::quantity_cast<units::millisecond>(timer.elapsed());

    utility::logger<"graphics">::debug("Loaded {} cube image: {} ({}x{}) in {:.2f}ms", is_hdr_file ? "HDR" : "LDR", path.string(), _extent.width, _extent.height, elapsed.value());
  }

  if (_extent.width == 0 || _extent.height == 0) {
    return;
  }

  _mip_levels = _mipmap ? mip_levels(_extent) : 1;

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
  create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
  create_image_view(_handle, _view, VK_IMAGE_VIEW_TYPE_CUBE, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  create_image_view(_handle, _array_view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  {
    SBX_PROFILE_SCOPE("cube_image::_upload");
    
    auto command_buffer = graphics::command_buffer{graphics::queue::type::graphics, true};
    
    if (from_file || _mipmap) {
      transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
    }
    
    auto staging_buffer = std::optional<graphics::staging_buffer>{};
    
    if (from_file) {
      staging_buffer.emplace(std::span{buffer.data(), buffer.size()});
      
      copy_buffer_to_image(command_buffer, *staging_buffer, _handle, _extent, _array_layers, 0);
    }
    
    if (_mipmap) {
      create_mipmaps(command_buffer, _handle, _extent, _format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _mip_levels, 0, _array_layers);
    } else if (from_file) {
      transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
    } else {
      transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
    }
    
    command_buffer.submit_idle();
  }
}

} // namespace sbx::graphics
