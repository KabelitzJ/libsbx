// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_
#define LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_

#include <filesystem>

#include <libsbx/utility/crc32.hpp>
#include <libsbx/utility/enum.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/reflection/description.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/resource_storage.hpp>
#include <libsbx/graphics/types.hpp>

#include <libsbx/graphics/images/image.hpp>

namespace sbx::graphics {

class image2d : public image {

public:

  image2d(const math::vector2u& extent, graphics::format format = graphics::format::r8g8b8a8_unorm, graphics::filter filter = graphics::filter::linear, graphics::address_mode address_mode = graphics::address_mode::repeat, graphics::usage usage = graphics::usage::color_attachment | graphics::usage::storage, graphics::samples samples = graphics::samples::count_1, bool anisotropic = false, bool mipmap = false, std::uint32_t array_layers = 1u);

  image2d(const std::filesystem::path& path, graphics::format format = graphics::format::r8g8b8a8_srgb, graphics::filter filter = graphics::filter::linear, graphics::address_mode address_mode = graphics::address_mode::repeat, bool anisotropic = false, bool mipmap = false);

  image2d(const math::vector2u& extent, graphics::format format, graphics::filter filter, memory::observer_ptr<const std::uint8_t> pixels);

  ~image2d() override = default;

  auto set_pixels(memory::observer_ptr<const std::uint8_t> pixels) -> void;

  auto name() const noexcept -> std::string override {
    return "Image 2D";
  }

private:

  enum class file_flags : std::uint16_t {
    none = 0,
    compressed = utility::bit_v<0>
  }; // enum class file_flags

  struct file_header {
    std::uint64_t magic;
    std::uint16_t version;
    std::uint16_t flags;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t channels;
    std::uint32_t uncompressed_size;
    std::uint32_t compressed_size;
  }; // struct file_header

  static_assert(sizeof(file_header) == 32u, "file_header layout changed");

  static constexpr auto file_magic = utility::make_magic<std::uint64_t>("SBXTEXTR");
  static constexpr auto file_version = std::uint16_t{1u};
  static constexpr auto binary_file_extension = std::string_view{".sbxtex"};

  auto _upload_pixels(const std::uint8_t* pixels, std::size_t size) -> void;

  auto _load(const std::filesystem::path& path = {}) -> void;

  auto _load_binary(const std::filesystem::path& path) -> void;

  static auto _process(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height, std::uint32_t channels, const std::uint8_t* pixels) -> void;

  bool _anisotropic;
	bool _mipmap;
  std::uint8_t _channels;

}; // class image2d

using image2d_handle = resource_handle<image2d>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_IMAGES_IMAGE2D_HPP_
