// SPDX-License-Identifier: MIT
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

image2d::image2d(const math::vector2u& extent, graphics::format format, graphics::filter filter, graphics::address_mode address_mode, VkImageUsageFlags usage, VkSampleCountFlagBits samples, bool anisotropic, bool mipmap, std::uint32_t array_layers)
: image{VkExtent3D{extent.x(), extent.y(), 1}, to_vk_enum<VkFilter>(filter), to_vk_enum<VkSamplerAddressMode>(address_mode), samples, (usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), to_vk_enum<VkFormat>(format), 1, array_layers},
  _anisotropic{anisotropic},
  _mipmap{mipmap} {
  _load();
}

// [TODO] KAJ 2023-07-28 : We use VK_FORMAT_R8G8B8A8_SRGB here because it best matches the STBI_rgb_alpha format that we load the image with.
image2d::image2d(const std::filesystem::path& path, graphics::format format, VkFilter filter, VkSamplerAddressMode address_mode, bool anisotropic, bool mipmap)
: image{VkExtent3D{0, 0, 1}, filter, address_mode, VK_SAMPLE_COUNT_1_BIT, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), to_vk_enum<VkFormat>(format), 1, 1},
  _anisotropic{anisotropic},
  _mipmap{mipmap} {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  _load(assets_module.resolve_path(path));
}

image2d::image2d(const math::vector2u& extent, graphics::format format, graphics::filter filter, memory::observer_ptr<const std::uint8_t> pixels)
: image2d{extent, format, filter, graphics::address_mode::repeat} {
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

  transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  copy_buffer_to_image(staging_buffer, _handle, _extent, _array_layers, 0);

  if (_mipmap) {
    create_mipmaps(_handle, _extent, _format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _mip_levels, 0, _array_layers);
  } else {
    transition_image_layout(_handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }
}

auto image2d::_upload_pixels(const std::uint8_t* pixels, const std::size_t size) -> void {
  _mip_levels = _mipmap ? mip_levels(_extent) : 1;

  create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D);
  create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
  create_image_view(_handle, _view, (_array_layers > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D), _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  auto command_buffer = graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

  transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

  auto staging_buffer = graphics::staging_buffer{std::span{pixels, size}};
  copy_buffer_to_image(command_buffer, staging_buffer, _handle, _extent, _array_layers, 0);

  if (_mipmap) {
    create_mipmaps(command_buffer, _handle, _extent, _format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _mip_levels, 0, _array_layers);
  } else {
    transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
  }

  command_buffer.submit_idle();
}

auto image2d::_load(const std::filesystem::path& path) -> void {
  _channels = channels_from_format(_format);

  auto from_file = false;
  auto staging_pixels = std::vector<std::uint8_t>{};

  if (!path.empty()) {
    from_file = true;

    const auto binary_path = std::filesystem::path{path}.replace_extension(binary_file_extension);

    if (std::filesystem::exists(binary_path)) {
      const auto source_time = std::filesystem::last_write_time(path);
      const auto binary_time = std::filesystem::last_write_time(binary_path);

      if (binary_time > source_time) {
        _load_binary(binary_path);
        return;
      }
    }

    auto timer = utility::timer{};

    stbi_set_flip_vertically_on_load(true);

    auto* pixels = stbi_load(path.string().c_str(), reinterpret_cast<std::int32_t*>(&_extent.width), reinterpret_cast<std::int32_t*>(&_extent.height), nullptr, STBI_rgb_alpha);

    if (!pixels) {
      throw std::runtime_error{fmt::format("Failed to load image: {}", path.string())};
    }

    if (_extent.width == 0 || _extent.height == 0) {
      stbi_image_free(pixels);
      throw std::runtime_error{fmt::format("Image '{}' has invalid dimensions: {}x{}", path.string(), _extent.width, _extent.height)};
    }

    _process(path, _extent.width, _extent.height, 4u, pixels);

    const auto buffer_size = _extent.width * _extent.height * 4u;
    staging_pixels.assign(pixels, pixels + buffer_size);

    stbi_image_free(pixels);

    utility::logger<"graphics">::debug("Loaded image: {} ({}x{}) in {:.2f}ms", path.string(), _extent.width, _extent.height, units::quantity_cast<units::millisecond>(timer.elapsed()).value());
  }

  if (from_file) {
    _upload_pixels(staging_pixels.data(), staging_pixels.size());
  } else {
    _mip_levels = _mipmap ? mip_levels(_extent) : 1;

    create_image(_handle, _allocation, _extent, _format, _samples, VK_IMAGE_TILING_OPTIMAL, _usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mip_levels, _array_layers, VK_IMAGE_TYPE_2D);
    create_image_sampler(_sampler, _filter, _address_mode, _anisotropic, _mip_levels);
    create_image_view(_handle, _view, (_array_layers > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D), _format, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);

    auto command_buffer = graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};
    transition_image_layout(command_buffer, _handle, _format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, _mip_levels, 0, _array_layers, 0);
    command_buffer.submit_idle();
  }
}

auto image2d::_load_binary(const std::filesystem::path& path) -> void {
  auto timer = utility::timer{};

  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};

  if (!file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open image file: {}", path.string())};
  }

  const auto file_size = static_cast<std::size_t>(file.tellg());
  file.seekg(0);

  if (file_size < sizeof(file_header) + sizeof(std::uint32_t)) {
    throw std::runtime_error{fmt::format("Image file too small: {}", path.string())};
  }

  auto buffer = std::vector<std::uint8_t>(file_size);
  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
  file.close();

  auto stored_checksum = std::uint32_t{0};
  std::memcpy(&stored_checksum, buffer.data() + file_size - sizeof(std::uint32_t), sizeof(std::uint32_t));

  const auto computed_checksum = utility::crc32(std::span{buffer.data(), file_size - sizeof(std::uint32_t)});

  if (stored_checksum != computed_checksum) {
    throw std::runtime_error{fmt::format("Image file checksum mismatch: '{}' (got {:08X}, expected {:08X})", path.string(), computed_checksum, stored_checksum)};
  }

  auto cursor = std::size_t{0};

  auto header = file_header{};
  
  std::memcpy(&header, buffer.data() + cursor, sizeof(file_header));
  cursor += sizeof(file_header);

  if (header.magic != file_magic) {
    throw std::runtime_error{fmt::format("Invalid image magic in '{}'", path.string())};
  }

  if (header.version != file_version) {
    throw std::runtime_error{fmt::format("Unsupported image version {} in '{}'", header.version, path.string())};
  }

  _extent.width = header.width;
  _extent.height = header.height;
  _channels = header.channels;

  const auto is_compressed = (header.flags & static_cast<std::uint16_t>(file_flags::compressed)) != 0u;

  auto pixels = std::vector<std::uint8_t>{};

  if (is_compressed) {
    pixels = utility::decompress<std::uint8_t, utility::compression_type::zstd>(std::span{reinterpret_cast<const char*>(buffer.data() + cursor), header.compressed_size}, header.uncompressed_size);
  } else {
    pixels.assign(buffer.data() + cursor, buffer.data() + cursor + header.uncompressed_size);
  }

  _upload_pixels(pixels.data(), pixels.size());

  utility::logger<"graphics">::debug("Loaded '{}': {}x{} in {:.2f}ms", path.string(), _extent.width, _extent.height, units::quantity_cast<units::millisecond>(timer.elapsed()).value());
}

auto image2d::_process(const std::filesystem::path& path, const std::uint32_t width, const std::uint32_t height, const std::uint32_t channels, const std::uint8_t* pixels) -> void {
  const auto output_path = std::filesystem::path{path}.replace_extension(binary_file_extension);

  const auto uncompressed_size = width * height * channels;

  const auto compressed = utility::compress<std::uint8_t, utility::compression_type::zstd>(std::span{pixels, uncompressed_size});

  auto header = file_header{};
  header.magic = file_magic;
  header.version = file_version;
  header.flags = static_cast<std::uint16_t>(file_flags::compressed);
  header.width = width;
  header.height = height;
  header.channels = channels;
  header.uncompressed_size = static_cast<std::uint32_t>(uncompressed_size);
  header.compressed_size   = static_cast<std::uint32_t>(compressed.size());

  const auto total_bytes = sizeof(file_header) + compressed.size() + sizeof(std::uint32_t);

  auto buffer = std::vector<std::uint8_t>{};
  buffer.reserve(total_bytes);

  const auto append_to = [](auto& target, const auto* source, const std::size_t size) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(source);
    target.insert(target.end(), bytes, bytes + size);
  };

  append_to(buffer, &header, sizeof(header));
  append_to(buffer, compressed.data(), compressed.size());

  const auto checksum = utility::crc32(buffer);
  append_to(buffer, &checksum, sizeof(checksum));

  auto file = std::ofstream{output_path, std::ios::binary};

  if (!file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open output file: {}", output_path.string())};
  }

  file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

  utility::logger<"graphics">::debug("Wrote '{}': {}x{} — {} bytes (compressed from {} bytes)", output_path.string(), width, height, buffer.size(), uncompressed_size);
}

} // namespace sbx::graphics
