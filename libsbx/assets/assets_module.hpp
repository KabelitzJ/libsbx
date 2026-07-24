// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>

namespace sbx::assets {

class assets_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, graphics::graphics_module>;

  assets_module();

  ~assets_module();

  /**
   * @brief Main thread. Loads (or returns the already-loaded) texture at @p path: reserves a
   * bindless index, decodes, queues the GPU upload, and returns a handle immediately.
   */
  auto load_texture(const std::filesystem::path& path) -> texture_handle;

  /**
   * @brief Main thread. Loads (or returns the already-loaded) mesh at @p path via fastgltf, queues
   * the GPU upload, and returns a handle immediately. Drawable once resident.
   */
  auto load_mesh(const std::filesystem::path& path) -> mesh_handle;

  auto create_material(const material::create_info& create_info) -> material_handle;

  /**
   * @brief Render thread. Turns queued texture loads into GPU images + bindless writes. Copies are
   * recorded by the caller's subsequent upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

  [[nodiscard]] auto white_texture() const noexcept -> texture_handle {
    return _white;
  }

  [[nodiscard]] auto normal_texture() const noexcept -> texture_handle {
    return _normal;
  }

  [[nodiscard]] auto black_texture() const noexcept -> texture_handle {
    return _black;
  }

  [[nodiscard]] auto magenta_texture() const noexcept -> texture_handle {
    return _magenta;
  }

  [[nodiscard]] auto material_buffer_address() const noexcept -> graphics::buffer::address_type {
    return _material_address;
  }

private:

  inline static constexpr auto material_capacity = std::uint32_t{1024u};

  auto _create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle;

  struct pending_texture_upload {
    std::uint32_t index;
    std::vector<std::byte> pixels;
    std::uint32_t width;
    std::uint32_t height;
    graphics::format format;
  }; // struct pending_texture_upload

  struct pending_mesh_upload {
    std::shared_ptr<mesh> record;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
  }; // struct pending_mesh_upload

  struct pending_material_upload {
    std::shared_ptr<material> record;
  }; // struct pending_material_upload

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, std::shared_ptr<texture>> _textures{};
  std::vector<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  std::unordered_map<std::string, std::shared_ptr<mesh>> _meshes{};
  std::vector<pending_mesh_upload> _pending_meshes{};

  std::vector<std::shared_ptr<material>> _materials{};
  std::vector<pending_material_upload> _pending_materials{};
  graphics::buffer_handle _material_buffer{};
  graphics::buffer::address_type _material_address{0u};
  std::uint32_t _material_count{0u};

  texture_handle _white{};
  texture_handle _normal{};
  texture_handle _black{};
  texture_handle _magenta{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
