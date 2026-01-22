// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_ATLAS_HPP_
#define LIBSBX_UI_ATLAS_HPP_

#include <memory>

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::ui {

class atlas {

public:

  atlas(std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& data);

  auto width() const noexcept -> std::uint32_t;

  auto height() const noexcept -> std::uint32_t;

  auto image() const noexcept -> const graphics::image2d&;

private:

  std::uint32_t _width;
  std::uint32_t _height;
  graphics::image2d_handle _image_id;

}; // class atlas

} // namespace sbx::ui

#endif // LIBSBX_UI_ATLAS_HPP_
