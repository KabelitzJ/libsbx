// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_TEXTURE_HPP_
#define LIBSBX_GRAPHICS_TEXTURE_HPP_

#include <string_view>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/asset.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::graphics {

/**
 * @brief Asset-layer view of a GPU texture.
 *
 * Owns the lifetime of the underlying image2d resource: when released (refcount reaches zero in the assets_module) the destructor enqueues the handle for deletion on the graphics_module's frame-fenced queue, so it is safe with frames in flight.
 *
 * @note Requires the assets_module to be torn down before the graphics_module (declare the dependency, or unload before graphics shutdown).
 */
class texture final : public assets::asset_base {

public:

  inline static constexpr auto type_name = std::string_view{"texture"};

  explicit texture(const image2d_handle handle)
  : _handle{handle} { }

  ~texture() override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    graphics_module.enqueue_resource_destruction(_handle);
  }

  auto type() const -> assets::asset_type override {
    return assets::asset_type::texture;
  }

  auto handle() const noexcept -> image2d_handle {
    return _handle;
  }

private:

  image2d_handle _handle;

}; // class texture

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_TEXTURE_HPP_