// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ASSETS_MODULE_HPP_
#define LIBSBX_ASSETS_ASSETS_MODULE_HPP_

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

  /**
   * @brief Render thread. Turns queued texture loads into GPU images + bindless writes. Copies are
   * recorded by the caller's subsequent upload_context::flush.
   */
  auto process_uploads(std::uint64_t frame_index) -> void;

  [[nodiscard]] auto is_resident(const texture_handle& texture) const -> bool;

  [[nodiscard]] auto is_resident(const mesh_handle& mesh) const -> bool;

private:

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

  mutable std::mutex _mutex{};

  std::unordered_map<std::string, std::shared_ptr<texture>> _textures{};
  std::vector<pending_texture_upload> _pending_textures{};
  std::unordered_map<std::uint32_t, graphics::image_handle> _images{};
  std::unordered_map<std::uint32_t, std::uint64_t> _resident_frame{};

  std::unordered_map<std::string, std::shared_ptr<mesh>> _meshes{};
  std::vector<pending_mesh_upload> _pending_meshes{};

}; // class assets_module

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ASSETS_MODULE_HPP_
