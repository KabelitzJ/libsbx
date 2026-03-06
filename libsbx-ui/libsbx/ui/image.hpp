// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_IMAGE_HPP_
#define LIBSBX_UI_IMAGE_HPP_

#include <libsbx/graphics/images/image.hpp>

#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class image : public element {

public:

  using source_type = std::variant<graphics::image2d_handle, std::string>;

  source_type source{};

  image() = default;

  ~image() override = default;

protected:

  auto submit(const math::vector2& screen_size) -> void override;

}; // class image

} // namespace sbx::ui

#endif // LIBSBX_UI_IMAGE_HPP_
