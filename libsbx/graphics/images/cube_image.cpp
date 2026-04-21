// SPDX-License-Identifier: MIT
#include <libsbx/graphics/images/cube_image.hpp>

#include <stb_image.h>

#include <libsbx/utility/logger.hpp>

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

auto cube_image::_load(const std::filesystem::path& path, const std::string& suffix) -> void {
  _channels = channels_from_format(_format);

  auto buffer = std::vector<std::uint8_t>{};
  auto offset = std::uint32_t{0};

  auto from_file = false;
  auto is_hdr = false;

  if (!path.empty()) {
    auto timer = utility::timer{};

    from_file = true;

    const auto first_path = path / fmt::format("{}{}", side_names[0], suffix);

    is_hdr = stbi_is_hdr(first_path.string().c_str());

    for (const auto& side : side_names) {
      const auto sub_path = path / fmt::format("{}{}", side, suffix);

      if (!std::filesystem::exists(sub_path)) {
        throw std::runtime_error{fmt::format("File does not exist: {}", sub_path.string())};
      }

      auto width = std::int32_t{0};
      auto height = std::int32_t{0};
      auto channels = std::int32_t{0};

      if (is_hdr) {
        utility::assert_that(stbi_is_hdr(sub_path.string().c_str()), "Cube image was determained to be HDR but face is not");
        utility::assert_that(_channels == 4u, "HDR Cube image requires 4 color channels");

        stbi_set_flip_vertically_on_load(false);

        auto* image_data = stbi_loadf(sub_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!image_data) {
          throw std::runtime_error{fmt::format("Failed to load HDR image: {}", sub_path.string())};
        }

        const auto size = width * height * _channels * sizeof(std::float_t);

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

        const auto size = width * height * _channels * sizeof(std::uint8_t);

        buffer.resize(buffer.size() + size);
        std::memcpy(buffer.data() + offset, image_data, size);
        offset += size;

        stbi_image_free(image_data);
      }

      _extent.width = std::max(_extent.width, static_cast<std::uint32_t>(width));
      _extent.height = std::max(_extent.height, static_cast<std::uint32_t>(height));
    }

    const auto elapsed = units::quantity_cast<units::millisecond>(timer.elapsed());

    utility::logger<"graphics">::debug("Loaded {} cube image: {} ({}x{}) in {:.2f}ms", is_hdr ? "HDR" : "LDR", path.string(), _extent.width, _extent.height, elapsed.value());
  }

  if (_extent.width == 0 || _extent.height == 0) {
    return;
  }

  _mip_levels = _mipmap ? mip_levels(_extent) : 1;

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
  create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
  create_image_view(_handle, _view, VK_IMAGE_VIEW_TYPE_CUBE, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  create_image_view(_handle, _array_view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  auto command_buffer = graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

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

} // namespace sbx::graphics
